// Entry point. Everything else lives behind zh::App so tests/tools could swap the impl later.
#include "zh/app.hpp"

#include <cstdlib>

int main() {
    zh::App app;
    return app.run();
}
