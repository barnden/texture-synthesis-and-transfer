#pragma once

std::random_device g_rd {};
std::mt19937 g_mtgen(g_rd());

int random(int max) { return std::uniform_int_distribution<>(0, max)(g_mtgen); }

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

    Coordinate& operator-=(Coordinate const& rhs)
    {
        x -= rhs.x;
        y -= rhs.y;

        return *this;
    }

    Coordinate operator+(Coordinate const& rhs) const
    {
        return Coordinate { *this } += rhs;
    }

    Coordinate operator-(Coordinate const& rhs) const
    {
        return Coordinate { *this } -= rhs;
    }

    friend std::ostream& operator<<(std::ostream& stream, Coordinate const& coord)
    {
        stream << '(' << coord.x << ", " << coord.y << ')';

        return stream;
    }
};

template <typename T>
class multivec {
private:
    std::vector<T> m_vec;
    size_t m_width {};
    size_t m_height {};

public:
    multivec()
        : m_vec() {};
    multivec(auto width, auto height)
        : m_width(width)
        , m_height(height)
    {
        m_vec.reserve(width * height);
    }

    multivec(auto width, auto height, T fill)
        : multivec(width, height)
    {
        m_vec = decltype(m_vec)(width * height, fill);
    }

    template <typename Numeric>
    T& operator[](Numeric idx)
    {
        return m_vec[idx];
    }

    template <typename Numeric>
    T& operator[](Numeric i, Numeric j)
    {
        auto idx = i + j * m_width;

        return m_vec[idx];
    }

    T& operator[](Coordinate coord)
    {
        auto idx = coord.x + coord.y * m_width;

        return m_vec[idx];
    }

    template <typename Numeric>
    T const& operator[](Numeric idx) const
    {
        return m_vec[idx];
    }

    template <typename Numeric>
    T const& operator[](Numeric i, Numeric j) const
    {
        auto idx = i + j * m_width;

        return m_vec[idx];
    }

    T const& operator[](Coordinate coord) const
    {
        auto idx = coord.x + coord.y * m_width;

        return m_vec[idx];
    }

    size_t size() const { return m_vec.size(); }

    void clear() { m_vec.clear(); }

    void fill(T value)
    {
        clear();

        m_vec = decltype(m_vec)(m_width * m_height, value);
    }
};

struct SSD {
    int ssd;
    Coordinate coord;

    friend bool operator<(SSD const& a, SSD const& b) { return a.ssd < b.ssd; }
    friend bool operator>(SSD const& a, SSD const& b) { return a.ssd > b.ssd; }
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
