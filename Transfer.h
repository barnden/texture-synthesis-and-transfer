#pragma once

#include "Quilt.h"

class Transfer : public Quilt {
private:
    Image const& m_constraint;

public:
    Transfer(Image const& texture, Image const& constraint)
        : Quilt(texture, constraint.width(), constraint.height())
        , m_constraint(constraint) {};

    [[gnu::flatten]] Coordinate random_overlapping_patch(Coordinate const& quxel, double alpha, int K) const
    {
        auto top_overlap = quxel.y >= m_chunk;
        auto left_overlap = quxel.x >= m_chunk;

        auto compute_ssd = [this](Coordinate const& quxel, Coordinate const& texel, Coordinate const& coord) {
            auto const& texture = m_texture[texel + coord];
            auto const& quilt = m_quilt[quxel + coord];

            return squared_difference(texture, quilt);
        };

        auto compute_error = [this](Coordinate const& quxel, Coordinate const& texel, Coordinate const& coord) {
            auto const& texture = m_texture[texel + coord];
            auto const& constraint = m_constraint[quxel + coord];

            return squared_difference(texture, constraint);
        };

        auto queue = std::priority_queue<SSD, std::vector<SSD>, std::less<SSD>> {};

        for (auto x = 0; x < m_texture.width() - m_patch; x++)
            for (auto y = 0; y < m_texture.height() - m_patch; y++) {
                auto patch = Coordinate { x, y };
                auto overlap = 0;

                if (left_overlap) {
                    auto max_u = std::min(m_overlap, m_quilt.width() - quxel.x);
                    auto max_v = std::min(m_patch, m_quilt.height() - quxel.y);

                    for (auto u = 0; u < max_u; u++)
                        for (auto v = 0; v < max_v; v++)
                            overlap += compute_ssd(quxel, patch, { u, v });
                }

                if (top_overlap) {
                    auto max_u = std::min(m_patch, m_quilt.width() - quxel.x);
                    auto max_v = std::min(m_overlap, m_quilt.height() - quxel.y);

                    for (auto u = 0; u < max_u; u++)
                        for (auto v = 0; v < max_v; v++)
                            overlap += compute_ssd(quxel, patch, { u, v });
                }

                if (left_overlap && top_overlap) {
                    auto max_u = std::min(m_overlap, m_quilt.width() - quxel.x);
                    auto max_v = std::min(m_overlap, m_quilt.height() - quxel.y);

                    for (auto u = 0; u < max_u; u++)
                        for (auto v = 0; v < max_v; v++)
                            overlap -= compute_ssd(quxel, patch, { u, v });
                }

                auto max_u = std::min(m_quilt.width(), quxel.x + m_patch);
                auto max_v = std::min(m_quilt.height(), quxel.y + m_patch);
                auto error = 0;

                for (auto u = 0; u < max_u - quxel.x; u++)
                    for (auto v = 0; v < max_v - quxel.y; v++)
                        error += compute_error(quxel, patch, { u, v });

                auto ssd = static_cast<int>(alpha * overlap) + static_cast<int>((1. - alpha) * error);

                if (queue.size() < K || queue.top().ssd > ssd) {
                    if (queue.size() == K)
                        queue.pop();

                    queue.push(SSD { ssd, patch });
                }
            }

        auto it = random(queue.size() - 1);
        for (auto i = 0; i < it; i++)
            queue.pop();

#if 0
        std::cout << "Match [badness: " << queue.top().ssd << "] Texture" << queue.top().coord << " -> Quilt" << quilt << '\n';
#endif

        return queue.top().coord;
    }

    [[gnu::flatten]] Coordinate seed_patch() const
    {
        auto const& reference = m_constraint[{}];
        auto min_ssd = std::numeric_limits<uint64_t>::max();
        auto min_ssd_coord = Coordinate {};

        for (auto x = 0; x < m_texture.width() - m_patch; x++)
            for (auto y = 0; y < m_texture.height() - m_patch; y++) {
                auto ssd = 0;

                for (auto u = 0; u < m_patch; u++)
                    for (auto v = 0; v < m_patch; v++) {
                        auto texel = Coordinate { x, y } + Coordinate { u, v };
                        ssd += squared_difference(reference, m_texture[texel]);
                    }

                if (min_ssd > ssd) {
                    min_ssd = ssd;
                    min_ssd_coord = { x, y };
                }
            }

        return min_ssd_coord;
    }

    [[gnu::flatten]] void transfer(double alpha, int K)
    {
        m_chunk = m_patch - m_overlap;

        auto max_chunk_y = (m_quilt.height() / m_chunk) + (m_quilt.height() % m_chunk != 0);
        auto max_chunk_x = (m_quilt.width() / m_chunk) + (m_quilt.width() % m_chunk != 0);

        for (auto u = 0; u < max_chunk_y; u++) {
            auto y = u * m_chunk;
            auto max_y = std::min(m_quilt.height() - 1, y + m_patch);

            for (auto v = 0; v < max_chunk_x; v++) {
                auto x = v * m_chunk;
                auto max_x = std::min(m_quilt.width() - 1, x + m_patch);

                auto quxel = Coordinate { x, y };

                if (u || v) {
                    auto patch = random_overlapping_patch(quxel, alpha, K);
                    auto mask = find_mask(quxel, patch, max_x, max_y);

                    copy_patch(quxel, patch, mask);
                }
            }
        }
    }

    [[gnu::flatten]] void synthesize(int patch_sz, int N, int K)
    {
        m_patch = patch_sz;
        m_overlap = std::max(m_patch / 6, 3);

        // Pick the closest match to the top-left patch in constraint from texture
        copy_patch({}, seed_patch());

        // Perform first pass using alpha = 0.1
        transfer(0.1, K);

        for (auto i = 1; i < N; i++) {
            auto alpha = .8 * (i / static_cast<double>(N - 1)) + .1;
            m_patch = static_cast<int>((2. / 3.) * m_patch);
            m_overlap = std::max(m_patch / 6, 3);

            transfer(alpha, K);
        }
    }
};
