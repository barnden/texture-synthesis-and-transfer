#pragma once

#include <algorithm>
#include <queue>
#include <random>
#include <vector>

#include "Image.h"

std::random_device g_rd {};
std::mt19937 g_mtgen(g_rd());

int random(int max) { return std::uniform_int_distribution<>(0, max)(g_mtgen); }

struct SSD {
    int ssd;
    Coordinate coord;

    friend bool operator<(SSD const& a, SSD const& b) { return a.ssd < b.ssd; }
    friend bool operator>(SSD const& a, SSD const& b) { return a.ssd > b.ssd; }
};

class Quilt {
protected:
    Image const& m_texture;
    Image m_quilt;

    int m_patch;
    int m_overlap;
    int m_chunk;

public:
    static constexpr int SYNTHESIS_RANDOM = 1;
    static constexpr int SYNTHESIS_SIMPLE = 2;
    static constexpr int SYNTHESIS_CUT = 3;

    Quilt(Image const& texture, int width, int height)
        : m_quilt(width, height)
        , m_texture(texture) {};

    [[gnu::flatten]] void copy_patch(Coordinate quilt, Coordinate texture)
    {
        auto max_y = std::min(m_quilt.height(), quilt.y + m_patch);
        auto max_x = std::min(m_quilt.width(), quilt.x + m_patch);

        for (auto i = 0; i < max_x - quilt.x; i++)
            for (auto j = 0; j < max_y - quilt.y; j++) {
                auto offset = Coordinate { i, j };
                m_quilt[quilt + offset] = m_texture[texture + offset];
            }
    }

    template <typename Mask>
    [[gnu::flatten]] void copy_patch(Coordinate quilt, Coordinate texture, Mask mask)
    {
        auto max_y = std::min(m_quilt.height(), quilt.y + m_patch);
        auto max_x = std::min(m_quilt.width(), quilt.x + m_patch);

        for (auto i = 0; i < max_x - quilt.x; i++)
            for (auto j = 0; j < max_y - quilt.y; j++) {
                if (!mask[i][j])
                    continue;

                auto offset = Coordinate { i, j };
                m_quilt[quilt + offset] = m_texture[texture + offset];
            }
    }

    Coordinate random_patch() const
    {
        auto p = random(m_texture.width() - m_patch);
        auto q = random(m_texture.height() - m_patch);

        return { p, q };
    }

    template <bool subtract, typename Metric>
    [[gnu::always_inline]] void compute_ssd(
        Metric&& compute_metric,
        auto& ssd,
        auto const& quxel,
        auto const& patch,
        auto const init_u,
        auto const init_v) const
    {
        auto const max_u = std::min(init_u, m_quilt.width() - quxel.x);
        auto const max_v = std::min(init_v, m_quilt.height() - quxel.y);

        for (auto u = 0; u < max_u; u++) {
            for (auto v = 0; v < max_v; v++) {
                auto const value = compute_metric(quxel, patch, { u, v });

                if constexpr (subtract) {
                    ssd -= value;
                } else {
                    ssd += value;
                }
            }
        }
    }

    template <typename Metric>
    [[gnu::flatten]] Coordinate random_overlapping_patch(Coordinate const& quxel, int K, Metric&& compute_metric) const
    {
        auto const top_overlap = quxel.y >= m_chunk;
        auto const left_overlap = quxel.x >= m_chunk;
        auto const corner_overlap = left_overlap && top_overlap;

        auto queue = std::priority_queue<SSD, std::vector<SSD>, std::less<SSD>> {};

        for (auto x = 0; x < m_texture.width() - m_patch; x++)
            for (auto y = 0; y < m_texture.height() - m_patch; y++) {
                auto patch = Coordinate { x, y };
                auto ssd = 0;

                if (left_overlap)
                    compute_ssd<false>(compute_metric, ssd, quxel, patch, m_overlap, m_patch);

                if (top_overlap)
                    compute_ssd<false>(compute_metric, ssd, quxel, patch, m_patch, m_overlap);

                if (corner_overlap)
                    compute_ssd<true>(compute_metric, ssd, quxel, patch, m_overlap, m_overlap);

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

    [[gnu::flatten]] Coordinate random_overlapping_patch(Coordinate const& quxel, int K) const
    {
        auto compute_ssd = [this](Coordinate const& quxel, Coordinate const& texel, Coordinate const& coord) {
            auto const& texture = m_texture[texel + coord];
            auto const& quilt = m_quilt[quxel + coord];

            return squared_difference(texture, quilt);
        };

        return random_overlapping_patch(quxel, K, compute_ssd);
    }

    [[gnu::flatten]] auto find_seam(
        Coordinate quxel,
        Coordinate texel,
        Coordinate overlap,
        bool vertical_seam = true) const
    {
        auto max_quxel = quxel + overlap;
        auto max_texel = texel + overlap;

        max_quxel.x = std::min(max_quxel.x, m_quilt.width());
        max_quxel.y = std::min(max_quxel.y, m_quilt.height());

        auto seam_height = max_quxel.y - quxel.y;
        auto seam_width = max_quxel.x - quxel.x;

        if (!vertical_seam)
            std::swap(seam_height, seam_width);

        auto seam = std::vector<Coordinate>(seam_height);

        auto energy = std::vector<std::vector<uint64_t>>(seam_height, std::vector<uint64_t>(seam_width, 0));
        auto matrix = std::vector<std::vector<std::pair<int, int>>>(seam_height, std::vector<std::pair<int, int>>(seam_width, std::make_pair(0, 0)));

        for (auto i = 0; i < seam_height; i++) {
            for (auto j = 0; j < seam_width; j++) {
                auto coord = vertical_seam ? Coordinate { j, i } : Coordinate { i, j };

                auto texture = m_texture[texel + coord];
                auto quilt = m_quilt[quxel + coord];

                energy[i][j] = squared_difference(quilt, texture);
            }
        }

        for (auto j = 0; j < seam_width; j++)
            matrix[0][j].first = energy[0][j];

        for (auto i = 1; i < seam_height; i++) {
            auto const& prev_row = matrix[i - 1];

            for (auto j = 0; j < seam_width; j++) {
                auto minval = prev_row[j].first;
                auto k = 0;

                if (j - 1 > 0 && prev_row[j - 1].first < minval) {
                    minval = prev_row[j - 1].first;
                    k = -1;
                }

                if (j + 1 < seam_width && prev_row[j + 1].first < minval) {
                    minval = prev_row[j + 1].first;
                    k = 1;
                }

                matrix[i][j] = std::make_pair(minval + energy[i][j], j + k);
            }
        }

        auto least_energy_element = std::min_element(
            matrix.back().cbegin(), matrix.back().cend(),
            [](auto const& a, auto const& b) { return a.first < b.first; });

        seam[seam_height - 1] = Coordinate {
            static_cast<int>(std::distance(matrix.back().cbegin(), least_energy_element)),
            seam_height - 1
        };

        if (!vertical_seam) {
            auto& back = seam[seam_height - 1];
            std::swap(back.x, back.y);
        }

        for (auto i = seam_height - 1; i-- > 0;) {
            auto j = least_energy_element->second;
            seam[i] = vertical_seam ? Coordinate { j, i } : Coordinate { i, j };

            least_energy_element = matrix[i].cbegin() + j;
        }

        return seam;
    }

    [[gnu::flatten]] auto find_mask(Coordinate quxel, Coordinate texel, int max_x, int max_y) const
    {
        auto mask = std::vector<std::vector<u_char>>(m_patch, std::vector<u_char>(m_patch, 1));

        if (quxel.x >= m_chunk) {
            auto overlap = Coordinate { m_overlap, max_y - quxel.y };
            auto seam = find_seam(quxel, texel, overlap);

            for (auto&& pixel : seam)
                for (auto i = 0; i <= pixel.x; i++)
                    mask[i][pixel.y] = 0;
        }

        if (quxel.y >= m_chunk) {
            auto overlap = Coordinate { max_x - quxel.x, m_overlap };
            auto seam = find_seam(quxel, texel, overlap, false);

            for (auto&& pixel : seam)
                for (auto i = 0; i <= pixel.y; i++)
                    mask[pixel.x][i] = 0;
        }

        return mask;
    }

    [[gnu::flatten]] void synthesize(int patch_sz, int overlap_sz, int K, int flag)
    {
        assert(patch_sz > overlap_sz);

        m_patch = patch_sz;
        m_overlap = overlap_sz;
        m_chunk = m_patch - m_overlap;

        auto max_chunk_y = m_quilt.height() / m_chunk + (m_quilt.height() % m_chunk != 0);
        auto max_chunk_x = m_quilt.width() / m_chunk + (m_quilt.width() % m_chunk != 0);

        for (auto u = 0; u < max_chunk_y; u++) {
            auto y = u * m_chunk;
            auto max_y = std::min(m_quilt.height() - 1, y + m_patch);

            for (auto v = 0; v < max_chunk_x; v++) {
                auto x = v * m_chunk;
                auto max_x = std::min(m_quilt.width() - 1, x + m_patch);

                auto quxel = Coordinate { x, y };

                if ((u || v) ^ (flag == SYNTHESIS_RANDOM)) {
                    auto patch = random_overlapping_patch(quxel, K);

                    if (flag == SYNTHESIS_CUT) {
                        auto mask = find_mask(quxel, patch, max_x, max_y);
                        copy_patch(quxel, patch, mask);
                    } else {
                        copy_patch(quxel, patch);
                    }
                } else {
                    copy_patch(quxel, random_patch());
                }
            }
        }
    }

    void write(std::string const& filename) const { m_quilt.write(filename); }
};
