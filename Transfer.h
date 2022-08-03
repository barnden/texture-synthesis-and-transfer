#pragma once

#include "Quilt.h"

class Transfer : public Quilt {
private:
    Image const& m_constraint;
    double m_alpha {};

public:
    Transfer(Image const& texture, Image const& constraint)
        : Quilt(texture, constraint.width(), constraint.height())
        , m_constraint(constraint) {};

    [[gnu::flatten, gnu::hot]] Coordinate random_overlapping_patch(Coordinate const& quxel, int K) const override
    {
        auto const top_overlap = quxel.y >= m_chunk;
        auto const left_overlap = quxel.x >= m_chunk;
        auto const corner_overlap = left_overlap && top_overlap;

        auto const ssd_metric = [this](Coordinate const& quxel, Coordinate const& texel, Coordinate const& coord) {
            auto const& texture = m_texture[texel + coord];
            auto const& quilt = m_quilt[quxel + coord];

            return squared_difference(texture, quilt);
        };

        auto queue = std::priority_queue<SSD, std::vector<SSD>, std::less<SSD>> {};

        for (auto x = 0; x < m_texture.width() - m_patch; x++)
            for (auto y = 0; y < m_texture.height() - m_patch; y++) {
                auto patch = Coordinate { x, y };
                auto overlap = 0;

                if (left_overlap)
                    compute_ssd<SSD_USE_ADDITION>(ssd_metric, overlap, quxel, patch, m_overlap, m_patch);

                if (top_overlap)
                    compute_ssd<SSD_USE_ADDITION>(ssd_metric, overlap, quxel, patch, m_patch, m_overlap);

                if (corner_overlap)
                    compute_ssd<SSD_USE_SUBTRACTION>(ssd_metric, overlap, quxel, patch, m_overlap, m_overlap);

                auto max = Coordinate {
                    std::min(m_quilt.width(), quxel.x + m_patch),
                    std::min(m_quilt.height(), quxel.y + m_patch)
                } - quxel;
                auto error = 0;

                compute_metric<SSD_USE_ADDITION>(
                    [this](Coordinate const& quxel, Coordinate const& texel, Coordinate const& coord) {
                        auto const& texture = m_texture[texel + coord];
                        auto const& constraint = m_constraint[quxel + coord];

                        return squared_difference(texture, constraint);
                    },
                    error, quxel, patch, max.x, max.y);

                auto ssd = static_cast<int>(m_alpha * overlap) + static_cast<int>((1. - m_alpha) * error);

                if (queue.size() < K || queue.top().ssd > ssd) {
                    if (queue.size() == K)
                        queue.pop();

                    queue.push(SSD { ssd, patch });
                }
            }

        auto it = random(queue.size() - 1);
        for (auto i = 0; i < it; i++)
            queue.pop();

#ifdef DBGLN
        std::cout << "Match [badness: " << queue.top().ssd << "] Texture" << queue.top().coord << " -> Quilt" << quxel << '\n';
#endif

        return queue.top().coord;
    }

    [[gnu::flatten, gnu::cold]] Coordinate seed_patch() const
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

    [[gnu::flatten]] void transfer(int K)
    {
        m_chunk = m_patch - m_overlap;

        m_max_chunk_y = (m_quilt.height() / m_chunk) + (m_quilt.height() % m_chunk != 0);
        m_max_chunk_x = (m_quilt.width() / m_chunk) + (m_quilt.width() % m_chunk != 0);

        m_status = decltype(m_status)(m_max_chunk_x, m_max_chunk_y, -1);
        m_completed = false;
        m_total_completed = 0;

        m_queue = decltype(m_queue) {}; // Clear queue
        m_queue.push({ 0, 0 });

        auto const max_threads = std::thread::hardware_concurrency();
        m_pool = decltype(m_pool) {};

        for (auto i = 0; i < max_threads; i++)
            m_pool.push_back(std::thread([this, K] -> void {
                return worker<SYNTHESIS_CUT>(K, false);
            }));

        while (is_busy()) { };

        cleanup();
    }

    [[gnu::flatten]] void synthesize(int patch_sz, int N, int K)
    {
        m_patch = std::max(patch_sz, 6);
        m_overlap = std::max(m_patch / 6, 3);

        // Pick the closest match to the top-left patch in constraint from texture
        copy_patch({}, seed_patch());

        // Perform first pass using alpha = 0.1
        m_alpha = 0.1;
        transfer(K);

        for (auto i = 1; i < N; i++) {
            m_alpha = .8 * (i / static_cast<double>(N - 1)) + .1;

            m_patch = static_cast<int>((2. / 3.) * m_patch);

            if (m_patch <= 3)
                return;

            m_overlap = std::max(m_patch / 6, 3);

            transfer(K);
        }
    }

    void write(std::string const& filename)
    {
        auto pixels = m_quilt.width() * m_quilt.height();

        for (auto x = 0; x < m_quilt.width(); x++)
            for (auto y = 0; y < m_quilt.height(); y++)
                m_quilt[x, y].ch.a = m_constraint[x, y].ch.a;

        m_quilt.write(filename);
    }
};
