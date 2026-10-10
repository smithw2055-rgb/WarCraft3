#pragma once
// Extended FDF frame types (ref parsers/fdf/frames/*).
//
// The local UI previously collapsed CHECKBOX / EDITBOX / LISTBOX /
// SCROLLBAR / POPUPMENU / TEXTAREA / STATUSBAR / SPRITE onto SimpleFrame.
// That renders but loses per-type state (checked, cursor, selection,
// progress) that Glue/menu FDFs and JASS dialog natives query. These thin
// subclasses preserve the state on top of SimpleFrame/UIFrame so existing
// layout/render paths keep working while parity improves.
#include "SimpleFrame.h"
#include "ButtonFrame.h"
#include <string>
#include <vector>

class CheckBoxFrame : public ButtonFrame {
public:
    CheckBoxFrame(const std::string& name, UIFrame* parent = nullptr)
        : ButtonFrame(name, parent) {}
    bool IsChecked() const { return checked_; }
    void SetChecked(bool v) { checked_ = v; }

    void SetCheckBoxCheckHighlight(UIFrame* f) { checkHighlight_ = f; }
    void SetCheckBoxDisabledCheckHighlight(UIFrame* f) { disabledCheckHighlight_ = f; }

    void Render(UiRenderer& renderer) override {
        if (!visible_) return;
        if (checkHighlight_) {
            checkHighlight_->SetVisible(checked_ && enabled_);
        }
        if (disabledCheckHighlight_) {
            disabledCheckHighlight_->SetVisible(checked_ && !enabled_);
        }
        ButtonFrame::Render(renderer);
    }

    UIFrame* OnMouseUp(float sx, float sy, int btn) override {
        if (btn == 0 && pressed_ && enabled_) {
            checked_ = !checked_;
        }
        return ButtonFrame::OnMouseUp(sx, sy, btn);
    }

private:
    bool checked_ = false;
    UIFrame* checkHighlight_ = nullptr;
    UIFrame* disabledCheckHighlight_ = nullptr;
};

class EditBoxFrame : public SimpleFrame {
public:
    EditBoxFrame(const std::string& name, UIFrame* parent = nullptr)
        : SimpleFrame(name, parent) {}
    const std::string& GetText() const { return text_; }
    void SetText(const std::string& t) { text_ = t; cursor_ = (int)text_.size(); }
    int GetCursor() const { return cursor_; }
    void SetCursor(int c) { cursor_ = c; }
    int GetMaxLength() const { return maxLength_; }
    void SetMaxLength(int n) { maxLength_ = n; }

private:
    std::string text_;
    int cursor_ = 0;
    int maxLength_ = 256;
};

class ListBoxFrame : public SimpleFrame {
public:
    ListBoxFrame(const std::string& name, UIFrame* parent = nullptr)
        : SimpleFrame(name, parent) {}
    void AddItem(const std::string& s) { items_.push_back(s); }
    void ClearItems() { items_.clear(); selected_ = -1; }
    int GetSelected() const { return selected_; }
    void SetSelected(int i) { selected_ = i; }
    size_t GetCount() const { return items_.size(); }
    const std::string& GetItem(size_t i) const { return items_.at(i); }

private:
    std::vector<std::string> items_;
    int selected_ = -1;
};

class ScrollBarFrame : public SimpleFrame {
public:
    ScrollBarFrame(const std::string& name, UIFrame* parent = nullptr)
        : SimpleFrame(name, parent) {}
    float GetValue() const { return value_; }
    void SetValue(float v) { value_ = v; }
    float GetMin() const { return min_; }
    float GetMax() const { return max_; }
    void SetRange(float lo, float hi) { min_ = lo; max_ = hi; }

private:
    float value_ = 0.0f;
    float min_ = 0.0f;
    float max_ = 1.0f;
};

class PopupMenuFrame : public ButtonFrame {
public:
    PopupMenuFrame(const std::string& name, UIFrame* parent = nullptr)
        : ButtonFrame(name, parent) {}
    void AddItem(const std::string& s) { items_.push_back(s); }
    void ClearItems() { items_.clear(); selected_ = -1; }
    int GetSelected() const { return selected_; }
    void SetSelected(int i) { selected_ = i; }
    size_t GetCount() const { return items_.size(); }
    const std::string& GetItem(size_t i) const { return items_.at(i); }
    bool IsOpen() const { return open_; }
    void SetOpen(bool v) { open_ = v; }
    void SetPopupMenu(UIFrame* menu) { popupMenu_ = menu; if (menu) menu->SetVisible(false); }
    UIFrame* GetPopupMenu() const { return popupMenu_; }

private:
    std::vector<std::string> items_;
    int selected_ = -1;
    bool open_ = false;
    UIFrame* popupMenu_ = nullptr;
};

class TextAreaFrame : public EditBoxFrame {
public:
    TextAreaFrame(const std::string& name, UIFrame* parent = nullptr)
        : EditBoxFrame(name, parent) {}
};

class StatusBarFrame : public SimpleFrame {
public:
    StatusBarFrame(const std::string& name, UIFrame* parent = nullptr)
        : SimpleFrame(name, parent) {}
    float GetValue() const { return value_; }
    void SetValue(float v) { value_ = v; }
    float GetMax() const { return max_; }
    void SetMax(float m) { max_ = m; }

private:
    float value_ = 1.0f;
    float max_ = 1.0f;
};

class SpriteFrame : public SimpleFrame {
public:
    SpriteFrame(const std::string& name, UIFrame* parent = nullptr)
        : SimpleFrame(name, parent) {}
    const std::string& GetModel() const { return model_; }
    void SetModel(const std::string& m) { model_ = m; }

private:
    std::string model_;
};

// MenuFrame (ref parsers/fdf/frames/MenuFrame): a popup list whose entries
// come from FDF MenuItem fields. Reuses the ListBox item store so selection
// and JASS dialog natives observe the same state.
class MenuFrame : public ListBoxFrame {
public:
    MenuFrame(const std::string& name, UIFrame* parent = nullptr)
        : ListBoxFrame(name, parent) {}
};
