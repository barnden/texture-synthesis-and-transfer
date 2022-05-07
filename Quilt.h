#pragma once
#include <queue>
#include <random>

#include "Image.h"

std::random_device g_rd {};
std::mt19937 g_mtgen(g_rd());

int random(int max)
{
    return std::uniform_int_distribution<>(0, max)(g_mtgen);
}

struct SSD {
    int ssd;
    Coordinate coord;

    friend bool operator<(SSD const& a, SSD const& b)
    {
        return a.ssd < b.ssd;
    }

    friend bool operator>(SSD const& a, SSD const& b)
    {
        return a.ssd > b.ssd;
    }
};

class Quilt {

private:
    Image const& m_texture;
    Image m_quilt;

    int m_patch;
    int m_overlap;

public:
    Quilt(Image const& texture, int width, int height)
        : m_quilt(width, height)
        , m_texture(texture) {};

    [[gnu::flatten]] void copy_patch(Coordinate quilt, Coordinate texture)
    {
        auto max_y = std::min(m_quilt.height() - 1, quilt.y + m_patch);
        auto max_x = std::min(m_quilt.width() - 1, quilt.x + m_patch);

        for (auto i = 0; i < max_x - quilt.x; i++)
            for (auto j = 0; j < max_y - quilt.y; j++) {
                auto color = m_texture.get_pixel(texture + Coordinate { i, j });
                m_quilt.set_pixel(quilt + Coordinate { i, j }, color);
            }
    }

    Coordinate random_patch()
    {
        auto p = random(m_texture.width() - m_patch);
        auto q = random(m_texture.height() - m_patch);

        return { p, q };
    }

    [[gnu::flatten]] Coordinate random_overlapping_patch(Coordinate quilt, int K)
    {
        auto top_overlap = quilt.y >= (m_patch - m_overlap);
        auto left_overlap = quilt.x >= (m_patch - m_overlap);

        auto compute_ssd = [this, &quilt](Coordinate patch, Coordinate coord) {
            auto texel = m_texture.get_pixel(patch + coord);
            auto quxel = m_quilt.get_pixel(quilt + coord);

            auto diff = absolute_difference(texel, quxel);

            return diff * diff;
        };

        auto queue = std::priority_queue<SSD, std::vector<SSD>, std::less<SSD>> {};

        for (auto x = 0; x < m_texture.width() - m_patch; x++)
            for (auto y = 0; y < m_texture.height() - m_patch; y++) {
                auto patch = Coordinate { x, y };
                auto ssd = 0;

                if (left_overlap) {
                    auto max_u = std::min(m_overlap, m_quilt.width() - quilt.x);
                    auto max_v = std::min(m_patch, m_quilt.height() - quilt.y);

                    for (auto u = 0; u < max_u; u++)
                        for (auto v = 0; v < max_v; v++)
                            ssd += compute_ssd(patch, { u, v });
                }

                if (top_overlap) {
                    auto max_u = std::min(m_patch, m_quilt.width() - quilt.x);
                    auto max_v = std::min(m_overlap, m_quilt.height() - quilt.y);

                    for (auto u = 0; u < max_u; u++)
                        for (auto v = 0; v < max_v; v++)
                            ssd += compute_ssd(patch, { u, v });
                }

                if (left_overlap && top_overlap) {
                    auto max_u = std::min(m_overlap, m_quilt.width() - quilt.x);
                    auto max_v = std::min(m_overlap, m_quilt.height() - quilt.y);

                    for (auto u = 0; u < max_u; u++)
                        for (auto v = 0; v < max_v; v++)
                            ssd -= compute_ssd(patch, { u, v });
                }

                if (queue.size() < K || queue.top().ssd > ssd) {
                    if (queue.size() == K)
                        queue.pop();

                    queue.push(SSD { ssd, patch });
                }
            }

        // std::cout << "Match [badness: " << min_ssd << "] Texture" << min_texel << " -> Quilt" << quilt << '\n';

        auto it = random(queue.size() - 1);
        for (auto i = 0; i < it; i++)
            queue.pop();

        return queue.top().coord;
    }

    [[gnu::flatten]] void synthesize(int patch_sz, int overlap_sz, int K)
    {
        assert(patch_sz > overlap_sz);

        m_patch = patch_sz;
        m_overlap = overlap_sz;

        auto chunk_sz = m_patch - overlap_sz;

        auto max_chunk_y = (m_quilt.height() / chunk_sz) + 1;
        auto max_chunk_x = (m_quilt.width() / chunk_sz) + 1;

        for (auto u = 0; u < max_chunk_y; u++) {
            auto y = u * chunk_sz;
            auto max_y = std::min(m_quilt.height() - 1, y + m_patch);

            for (auto v = 0; v < max_chunk_x; v++) {
                auto x = v * chunk_sz;
                auto max_x = std::min(m_quilt.width() - 1, x + m_patch);

                auto patch = Coordinate { 0, 0 };

                if (u || v) {
                    patch = random_overlapping_patch({ x, y }, K);
                } else {
                    patch = random_patch();
                }

                copy_patch({ x, y }, patch);
            }
        }
    }

    void write(std::string const& filename) const { m_quilt.write(filename); }
};
