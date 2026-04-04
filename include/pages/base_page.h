#pragma once

class BasePage
{
public:
    virtual ~BasePage() {}
    virtual void render() = 0;
    virtual void input();
};