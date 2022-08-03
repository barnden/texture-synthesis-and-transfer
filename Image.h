#pragma once

#include <array>
#include <cassert>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <png.h>

#include "Utility.h"

class Image {
private:
    std::string m_filename {};
    int m_width {};
    int m_height {};

    png_byte m_color_type {};
    png_byte m_bit_depth {};

    multivec<RGBA> m_image;

public:
    Image() {};

    Image(int width, int height)
        : m_width(width)
        , m_height(height)
    {
        m_color_type = PNG_COLOR_TYPE_RGBA;
        m_bit_depth = 8;

        m_image = decltype(m_image)(m_height, m_width, 0);
    }

    Image(std::string const& filename)
        : m_filename { filename }
    {
        open();
    }

    RGBA& operator[](int x, int y)
    {
        assert(y >= 0 && y < m_height);
        assert(x >= 0 && x < m_width);

        return m_image[x + y * m_width];
    }

    RGBA& operator[](Coordinate const& coord)
    {
        return (*this)[coord.x, coord.y];
    }

    RGBA const& operator[](int x, int y) const
    {
        assert(y >= 0 && y < m_height);
        assert(x >= 0 && x < m_width);

        return m_image[x + y * m_width];
    }

    RGBA const& operator[](Coordinate const& coord) const
    {
        return (*this)[coord.x, coord.y];
    }

    int height() const { return m_height; }
    int width() const { return m_width; }

    void open()
    {
        assert(m_filename.size());

        open(m_filename);
    }

    void open(std::string const& filename)
    {
        auto file = fopen(filename.c_str(), "rb");
        assert(file);

        auto png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        assert(png);

        auto info = png_create_info_struct(png);
        assert(info);

        if (setjmp(png_jmpbuf(png)))
            abort();

        png_init_io(png, file);
        png_read_info(png, info);

        m_width = png_get_image_width(png, info);
        m_height = png_get_image_height(png, info);
        m_color_type = png_get_color_type(png, info);
        m_bit_depth = png_get_bit_depth(png, info);

        if (m_bit_depth == 16)
            png_set_strip_16(png);

        if (m_color_type == PNG_COLOR_TYPE_PALETTE)
            png_set_palette_to_rgb(png);

        if (m_color_type == PNG_COLOR_TYPE_GRAY && m_bit_depth < 8)
            png_set_expand_gray_1_2_4_to_8(png);

        if (png_get_valid(png, info, PNG_INFO_tRNS))
            png_set_tRNS_to_alpha(png);

        if (m_color_type == PNG_COLOR_TYPE_RGB || m_color_type == PNG_COLOR_TYPE_GRAY || m_color_type == PNG_COLOR_TYPE_PALETTE)
            png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

        if (m_color_type == PNG_COLOR_TYPE_GRAY || m_color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
            png_set_gray_to_rgb(png);

        png_read_update_info(png, info);

        auto rows = (png_bytep*)malloc(sizeof(png_bytep) * m_height);

        for (auto i = 0; i < m_height; i++)
            rows[i] = (png_byte*)malloc(png_get_rowbytes(png, info));

        png_read_image(png, rows);

        fclose(file);

        png_destroy_read_struct(&png, &info, NULL);

        m_image = decltype(m_image)(m_height, m_width, 0);

        for (auto i = 0; i < m_height; i++) {
            auto row = rows[i];

            for (auto j = 0; j < m_width; j++) {
                auto const* const color = &(row[j * 4]);
                auto& pixel = m_image[j + i * m_width];

                pixel.ch.r = color[0];
                pixel.ch.g = color[1];
                pixel.ch.b = color[2];
                pixel.ch.a = color[3];
            }

            free(row);
        }

        free(rows);
    }

    void write(bool alpha = true) const
    {
        assert(m_filename.size());

        write(m_filename, alpha);
    }

    void write(std::string const& filename, bool alpha = true) const
    {
        auto file = fopen(filename.c_str(), "wb");
        assert(file);

        auto png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        assert(png);

        auto info = png_create_info_struct(png);
        assert(info);

        if (setjmp(png_jmpbuf(png)))
            exit(EXIT_FAILURE);

        png_init_io(png, file);

        auto color_type = alpha ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB;

        png_set_IHDR(png, info, m_width, m_height, 8, color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
        png_write_info(png, info);

        if (!alpha)
            png_set_filler(png, 0, PNG_FILLER_AFTER);

        assert(m_image.size());

        auto rows = (png_bytep*)malloc(sizeof(png_bytep) * m_height);

        for (auto i = 0; i < m_height; i++)
            rows[i] = (png_byte*)malloc(png_get_rowbytes(png, info));

        for (auto i = 0; i < m_height; i++) {
            auto row = rows[i];

            for (auto j = 0; j < m_width; j++) {
                auto const& color = m_image[j + i * m_width];
                auto* const pixel = &(row[j * 4]);

                pixel[0] = color.ch.r;
                pixel[1] = color.ch.g;
                pixel[2] = color.ch.b;
                pixel[3] = color.ch.a;
            }
        }

        png_write_image(png, rows);
        png_write_end(png, NULL);

        for (auto i = 0; i < m_height; i++)
            free(rows[i]);

        free(rows);

        fclose(file);

        png_destroy_write_struct(&png, &info);
    }
};
