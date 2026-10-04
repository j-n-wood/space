#pragma once

class Modal
{
protected:
    bool visible{false};
    int top;
    int left;

public:
    bool isVisible() const { return visible; }
    virtual void activate();
    virtual void deactivate();
    virtual void renderModal();
    virtual void render();
    Modal(int l, int t) : top(t), left(l) { visible = false; }
    virtual ~Modal() = default;
};