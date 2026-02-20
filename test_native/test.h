#pragma once

struct Test {
    const char *name;
    bool (*func)();
};
