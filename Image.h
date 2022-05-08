#pragma once

#include <array>
#include <cassert>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <png.h>

class Coordinate {
public:
    int x = 0;
    int y = 0;

    Coordinate() {};

    Coordinate(int x, int y)
        : x(x)
        , y(y) {};

    Coordinate(int x)
        : x(x)
        , y(x) {};

    Coordinate& operator+=(Coordinate const& rhs)
    {
        x += rhs.x;
        y += rhs.y;

        return *this;
    }

    Coordinate operator+(Coordinate const& rhs) const
    {
        return Coordinate { *this } += rhs;
    }

    friend std::ostream& operator<<(std::ostream& stream, Coordinate const& coord)
    {
        stream << "(" << coord.x << ", " << coord.y << ')';

        return stream;
    }
};

union RGB {
    uint32_t value;

    struct {
        png_byte unused;
        png_byte r;
        png_byte g;
        png_byte b;
    } ch;

    RGB() {};

    // clang-format off
    RGB(uint32_t color) {
        ch.r = (color >> 16) & 0xFF;
        ch.g = (color >>  8) & 0xFF;
        ch.b =  color        & 0xFF;
    }
    // clang-format on

    RGB(png_byte r, png_byte g, png_byte b)
    {
        ch.r = r;
        ch.g = g;
        ch.b = b;
    }
};

union RGBA {
    uint32_t value;

    struct {
        png_byte r;
        png_byte g;
        png_byte b;
        png_byte a;
    } ch;

    RGBA() {};

    // clang-format off
    RGBA(uint32_t color) {
        ch.r = (color >> 24) & 0xFF;
        ch.g = (color >> 16) & 0xFF;
        ch.b = (color >>  8) & 0xFF;
        ch.a =  color        & 0xFF;
    }
    // clang-format on

    RGBA(RGB color)
    {
        ch.r = color.ch.r;
        ch.g = color.ch.g;
        ch.b = color.ch.b;
    }

    RGBA(png_byte r, png_byte g, png_byte b, png_byte a)
    {
        ch.r = r;
        ch.g = g;
        ch.b = b;
        ch.a = a;
    }

    // clang-format off
    friend uint64_t squared_difference(RGBA const& first, RGBA const& second) {
        auto acc = 0ll;

        acc += (first.ch.r - second.ch.r)
             + (first.ch.g - second.ch.g)
             + (first.ch.b - second.ch.b);

        acc = std::abs(acc);

        return acc * acc;
    }
    // clang-format on

    friend std::ostream& operator<<(std::ostream& stream, RGBA const& rgba)
    {
        stream << "RGBA(" << +rgba.ch.r << ", " << +rgba.ch.g << ", " << +rgba.ch.b << ", " << +rgba.ch.a << ')';

        return stream;
    }
};

class Image {
private:
    std::string m_filename {};
    int m_width {};
    int m_height {};

    png_byte m_color_type {};
    png_byte m_bit_depth {};

    std::array<std::vector<std::vector<png_byte>>, 4> m_image;

public:
    Image() {};

    Image(int width, int height)
        : m_width(width)
        , m_height(height)
    {
        m_color_type = PNG_COLOR_TYPE_RGBA;
        m_bit_depth = 8;

        for (auto&& channel : m_image)
            channel = std::vector<std::vector<png_byte>>(m_height, std::vector<png_byte>(m_width, 0));
    }

    Image(std::string const& filename)
        : m_filename { filename }
    {
        open();
    }

    std::vector<std::vector<png_byte>>& operator[](int channel)
    {
        assert(channel >= 0 && channel < 4);

        return m_image[channel];
    }

    std::vector<std::vector<png_byte>>& r() { return m_image[0]; }
    std::vector<std::vector<png_byte>>& g() { return m_image[1]; }
    std::vector<std::vector<png_byte>>& b() { return m_image[2]; }
    std::vector<std::vector<png_byte>>& a() { return m_image[3]; }

    int height() const { return m_height; }
    int width() const { return m_width; }

    // clang-format off
    void set_pixel_rgba(Coordinate coord, RGBA color) {
        auto y = coord.y;
        auto x = coord.x;

        assert(y >= 0 && y < m_height);
        assert(x >= 0 && x < m_width);

        m_image[0][y][x] = color.ch.r;
        m_image[1][y][x] = color.ch.g;
        m_image[2][y][x] = color.ch.b;
        m_image[3][y][x] = color.ch.a;
    }

    void set_pixel(Coordinate coord, RGBA rgb) {
        auto y = coord.y;
        auto x = coord.x;

        assert(y >= 0 && y < m_height);
        assert(x >= 0 && x < m_width);

        m_image[0][y][x] = rgb.ch.r;
        m_image[1][y][x] = rgb.ch.g;
        m_image[2][y][x] = rgb.ch.b;
        m_image[3][y][x] = 0xFF;
    }

    RGBA get_pixel(Coordinate coord) const {
        auto y = coord.y;
        auto x = coord.x;

        assert(y >= 0 && y < m_height);
        assert(x >= 0 && x < m_width);

        auto color = RGBA {};

        color.ch.r = m_image[0][y][x];
        color.ch.g = m_image[1][y][x];
        color.ch.b = m_image[2][y][x];
        color.ch.a = m_image[3][y][x];

        return color;
    }
    // clang-format on

    RGBA operator[](Coordinate coord) const
    {
        return get_pixel(coord);
    }

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

        for (auto&& channel : m_image)
            channel = std::vector<std::vector<png_byte>>(m_height, std::vector<png_byte>(m_width, 0));

        for (auto i = 0; i < m_height; i++) {
            auto row = rows[i];

            for (auto j = 0; j < m_width; j++) {
                auto pixel = &(row[j * 4]);

                for (auto k = 0; k < 4; k++)
                    m_image[k][i][j] = pixel[k];
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

        assert(m_image[0].size());

        auto rows = (png_bytep*)malloc(sizeof(png_bytep) * m_height);

        for (auto i = 0; i < m_height; i++)
            rows[i] = (png_byte*)malloc(png_get_rowbytes(png, info));

        for (auto i = 0; i < m_height; i++) {
            auto row = rows[i];

            for (auto j = 0; j < m_width; j++) {
                auto pixel = &(row[j * 4]);

                for (auto k = 0; k < 4; k++)
                    pixel[k] = m_image[k][i][j];
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
