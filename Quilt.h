#pragma once
#include <random>

#include "Image.h"

std::random_device g_rd {};
std::mt19937 g_mtgen(g_rd());

int random(int max)
{
    return std::uniform_int_distribution<>(0, max)(g_mtgen);
}

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

    void copy_patch(Coordinate quilt, Coordinate texture)
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

    Coordinate random_overlapping_patch(Coordinate quilt)
    {
        auto top_overlap = quilt.y >= (m_patch - m_overlap);
        auto left_overlap = quilt.x >= (m_patch - m_overlap);

        auto min_ssd = std::numeric_limits<uint64_t>::max();
        auto min_texel = Coordinate { 0, 0 };

        for (auto x = 0; x < m_texture.width() - m_patch; x++)
            for (auto y = 0; y < m_texture.height() - m_patch; y++) {
                auto patch = Coordinate { x, y };
                auto ssd = 0ull;

                if (left_overlap) {
                    auto max_u = std::min(m_overlap, m_quilt.width() - quilt.x);
                    auto max_v = std::min(m_patch, m_quilt.height() - quilt.y);

                    for (auto u = 0; u < max_u; u++)
                        for (auto v = 0; v < max_v; v++) {
                            auto coord = Coordinate { u, v };

                            auto texel = m_texture.get_pixel(patch + coord);
                            auto quxel = m_quilt.get_pixel(quilt + coord);

                            auto diff = absolute_difference(texel, quxel);

                            ssd += diff * diff;
                        }
                }

                if (top_overlap) {
                    auto max_u = std::min(m_patch, m_quilt.width() - quilt.x);
                    auto max_v = std::min(m_overlap, m_quilt.height() - quilt.y);

                    for (auto u = 0; u < max_u; u++)
                        for (auto v = 0; v < max_v; v++) {
                            auto coord = Coordinate { u, v };

                            auto texture_px = m_texture.get_pixel(patch + coord);
                            auto quilt_px = m_quilt.get_pixel(quilt + coord);

                            auto diff = absolute_difference(texture_px, quilt_px);
                            ssd += diff * diff;
                        }
                }

                if (min_ssd > ssd) {
                    min_ssd = ssd;
                    min_texel = Coordinate { x, y };
                }
            }

        std::cout << "Match [badness: " << min_ssd << "] Texture" << min_texel << " -> Quilt" << quilt << '\n';

        return min_texel;
    }

    void synthesize(int patch_sz, int overlap_sz, int K)
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

                auto patch = random_patch();

                if (u || v) {
                    patch = random_overlapping_patch({ x, y });
                }

                copy_patch({ x, y }, patch);
            }
        }
    }

    void write(std::string const& filename) const { m_quilt.write(filename); }
};
