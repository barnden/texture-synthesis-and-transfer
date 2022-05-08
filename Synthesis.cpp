#include <iostream>
#include <random>

#include "Transfer.h"

int main(int argc, char** argv)
{
    assert(argc > 1);

    auto texture_name = std::string { argv[1] };
    auto texture = Image("samples/" + texture_name + ".png");

    if (argc == 5) {
        auto quilt = Quilt(texture, 384, 384);

        quilt.synthesize(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), Quilt::SYNTHESIS_CUT);

        quilt.write("results/" + texture_name + ".png");
    } else if (argc == 6) {
        auto constraint_name = std::string { argv[2]};
        auto constraint = Image("samples/" + constraint_name + ".png");

        auto transfer = Transfer(texture, constraint);

        transfer.synthesize(atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));

        transfer.write("results/" + constraint_name + "_" + texture_name + ".png");
    }

    return 0;
}
