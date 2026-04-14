#include "DialogBuilder.h"

DialogBuilder::DialogBuilder()
{
}

DialogBuilder::~DialogBuilder()
{
}

DialogBuilder* DialogBuilder::AddTextBox(const string& str, bool center)
{
    m_str += "add_textbox|" + str + "|";

    if(center) {
        m_str += "center|\n";
    }
    else {
        m_str += "\n";
    }

    return this;
}

DialogBuilder* DialogBuilder::AddLabel(const string& str, bool center)
{
    return AddTextBox(str, center);
}

DialogBuilder* DialogBuilder::AddCustomLine(const string& line)
{
    m_str += line;
    if(m_str.empty() || m_str.back() != '\n') {
        m_str += "\n";
    }

    return this;
}

DialogBuilder* DialogBuilder::AddLabelWithIcon(const string& str, uint16 itemID, bool big, bool center)
{
    m_str += "add_label_with_icon|";

    if(big) m_str += "big|";
    else m_str += "small|";

    m_str += str + "|";

    if(center) m_str += "center|";
    else m_str += "left|";

    m_str += ToString(itemID) + "|\n";
    return this;
}

DialogBuilder* DialogBuilder::AddButton(const string& buttonID, const string& text, const string& flags)
{
    m_str += "add_button|" + buttonID  + "|" + text + "|" + flags + "|0|0|\n"; // urlPrompt?
    return this;
}

DialogBuilder* DialogBuilder::AddTextInput(const string& buttonID, const string& text, const string& placeholder, uint32 inputMaxLength)
{
    m_str += "add_text_input|" + buttonID + "|" + text + "|" + placeholder + "|" + ToString(inputMaxLength) + "|\n";
    return this;
}

DialogBuilder* DialogBuilder::AddTextInputPassword(const string& buttonID, const string& text, const string& placeholder, uint32 inputMaxLength)
{
    m_str += "add_text_input_password|" + buttonID + "|" + text + "|" + placeholder + "|" + ToString(inputMaxLength) + "|\n";
    return this;
}

DialogBuilder* DialogBuilder::EndDialog(const string& dialogID, const string& acceptText, const string& cancelText)
{
    m_str += "end_dialog|" + dialogID + "|" + cancelText + "|" + acceptText + "|\n";
    return this;
}

DialogBuilder* DialogBuilder::AddQuickExit()
{
    m_str += "add_quick_exit|\n";
    return this;
}

DialogBuilder* DialogBuilder::AddItemPicker(const string& pickerID, const string& label, const string& description)
{
    m_str += "add_item_picker|" + pickerID + "|" + label + "|" + description + "|\n";
    return this;
}

DialogBuilder* DialogBuilder::SetDefaultColor(char colorID)
{
    m_str += "set_default_color|`";
    m_str += colorID;
    m_str += "|\n";
    return this;
}

DialogBuilder* DialogBuilder::AddSpacer(bool big)
{
    m_str += "add_spacer|";

    if(big) m_str += "big|\n";
    else m_str += "small|\n";

    return this;
}

DialogBuilder* DialogBuilder::AddCheckBox(const string& boxID, const string& text, bool active)
{
    m_str += "add_checkbox|" + boxID + "|" + text + "|";
    m_str += active ? "1|\n" : "0|\n";

    return this;
}

DialogBuilder* DialogBuilder::AddPlayerPicker(const string& pickerID, const string& buttonText)
{
    m_str += "add_player_picker|" + pickerID + "|" + buttonText + "|\n";
    return this;
}

DialogBuilder* DialogBuilder::EmbedData(const string& name, const string& value)
{
    m_str += "embed_data|" + name + "|" + value + "\n";
    return this;
}
