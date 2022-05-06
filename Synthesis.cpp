#include <iostream>
#include <random>

#include "Image.h"

auto g_rd = std::random_device {};
auto g_mtgen = std::mt19937 { g_rd() };

int32_t random(int32_t max)
{
    return std::uniform_int_distribution<>(0, max)(g_mtgen);
}

void copy_patch(Image& out, Coordinate coord_out, Image const& in, Coordinate coord_in, int patch_sz)
{
    auto max_y = std::min(out.height() - 1, coord_out.y + patch_sz);
    auto max_x = std::min(out.width() - 1, coord_out.x + patch_sz);

    for (auto i = 0; i < max_x - coord_out.x; i++)
        for (auto j = 0; j < max_y - coord_out.y; j++) {
            auto color = in.get_pixel(coord_in + Coordinate { i, j });
            out.set_pixel(coord_out + Coordinate { i, j }, color);
        }
}

void copy_random_patch(Image& out, Image const& in, Coordinate coord, int patch_sz)
{
    auto p = random(in.width() - patch_sz);
    auto q = random(in.height() - patch_sz);

    copy_patch(out, coord, in, { p, q }, patch_sz);
}

void quilt(Image const& texture, Image& quilt, int patch_sz, int overlap, int K)
{
    assert(patch_sz > overlap);

    auto chunk_sz = patch_sz - overlap;

    for (auto u = 0; u < (quilt.height() / chunk_sz) + 1; u++) {
        auto y = u * chunk_sz;
        auto max_y = std::min(quilt.height() - 1, y + patch_sz);

        for (auto v = 0; v < (quilt.width() / chunk_sz) + 1; v++) {
            auto x = v * chunk_sz;
            auto max_x = std::min(quilt.width() - 1, x + patch_sz);

            if (!u && !v)
                copy_random_patch(quilt, texture, { 0, 0 }, patch_sz);

            copy_random_patch(quilt, texture, { x, y }, patch_sz);
        }
    }
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    auto fname = std::string { argv[1] };
    auto texture = Image("samples/" + fname + ".png");
    auto output = Image(384, 384);

    quilt(texture, output, 20, 3, 3);

    output.write("results/" + fname + ".png");

    return 0;
}
