#include <iostream>
#include <random>

#include "Quilt.h"
#include "Transfer.h"

#include <getopt.h>

int main(int argc, char** argv)
{
    auto texture_path = std::string {};
    auto constraint_path = std::string {};
    auto outfile = std::string {};

    auto method = Quilt::SYNTHESIS_CUT;
    auto patch_size = 0;
    auto overlap = 0;
    auto samples = 0;
    auto depth = 1;

    auto width = 384;
    auto height = 384;

    option longopts[11] = {
        option { "texture", 1, NULL, 't' },
        option { "constraint", 1, NULL, 'c' },
        option { "outfile", 1, NULL, 'O' },
        option { "method", 1, NULL, 'm' },
        option { "patch-size", 1, NULL, 'p' },
        option { "overlap", 1, NULL, 'o' },
        option { "samples", 1, NULL, 'K' },
        option { "width", 1, NULL, 'w' },
        option { "height", 1, NULL, 'h' },
        option { "depth", 1, NULL, 'd' },
        NULL
    };

    auto option = '\0';

    while ((option = getopt_long(argc, argv, "t:c:O:m:p:o:K:w:h:d:", longopts, 0)) != -1) {
        switch (option) {
        case 't':
            texture_path = { optarg };
            break;
        case 'c':
            constraint_path = { optarg };
            break;
        case 'O':
            outfile = { optarg };
            break;

        case 'm':
            method = atoi(optarg);
            break;
        case 'p':
            patch_size = atoi(optarg);
            break;
        case 'l':
            overlap = atoi(optarg);
            break;
        case 'K':
            samples = atoi(optarg);
            break;
        case 'w':
            width = atoi(optarg);
            break;
        case 'h':
            height = atoi(optarg);
            break;
        case 'd':
            depth = atoi(optarg);
            break;
        }
    }

    if (texture_path.empty())
        throw std::runtime_error("No texture name supplied.");

    if (outfile.empty())
        outfile = "output.png";

    if (patch_size <= 0)
        patch_size = 18;

    if (overlap <= 0)
        overlap = patch_size / 6;

    if (samples <= 0)
        samples = 3;

    auto texture = Image(texture_path);
    if (constraint_path.empty()) {
        // Texture synthesis if no constraint
        auto quilt = Quilt(texture, width, height);

        quilt.synthesize(patch_size, overlap, samples, method);
        quilt.write(outfile);
    } else {
        auto constraint = Image(constraint_path);
        auto transfer = Transfer(texture, constraint);

        transfer.synthesize(patch_size, depth, samples);
        transfer.write(outfile);
    }

    return 0;
}
