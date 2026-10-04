#pragma once
#include "pages/modal.h"

class MessageBox : public Modal
{
    char buf[256]; // storage of message s.t. composed messages do not vanish
public:
    MessageBox(int l, int t, const char *message);

    void render();
};