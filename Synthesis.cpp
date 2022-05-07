#include <iostream>
#include <random>

#include "Image.h"
#include "Quilt.h"

int main(int argc, char** argv)
{
    assert(argc == 2);

    auto fname = std::string { argv[1] };
    auto texture = Image("samples/" + fname + ".png");
    auto quilt = Quilt(texture, 384, 384);

    // quilt(texture, output, 20, 3, 3);

    quilt.synthesize(21, 3, 3);

    quilt.write("results/" + fname + ".png");

    return 0;
}
