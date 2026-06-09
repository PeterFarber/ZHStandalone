// Entry point. Everything else lives behind zh::App so tests/tools could swap the impl later.
#include "zh/app.hpp"

int main(int argc, char **argv) {
    zh::App app;
    return app.run(argc, argv);
}
