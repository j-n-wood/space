#pragma once

#include <cstdint>

enum class ModalResult : uint8_t
{
    None,
    OK,
    Yes,
    No
};

enum class ModalButtons : uint8_t
{
    OK,   // single OK button
    YesNo // two buttons: Yes and No
};

class Modal
{
protected:
    int top;
    int left;

    ModalResult result{ModalResult::None};
    ModalButtons buttons{ModalButtons::OK};

    bool closed; // to be closed in next update pass

    void close(ModalResult r);

public:
    virtual void renderModal();
    virtual void render();
    Modal(int l, int t) : top(t), left(l), closed(false) {}
    virtual ~Modal() = default;
    bool isClosed() const { return closed; }
    ModalResult getResult() const { return result; }
    virtual void onClose();
    virtual void update(const float delta);
};