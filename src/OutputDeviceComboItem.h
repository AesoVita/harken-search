#pragma once

#include <string>

struct OutputDeviceComboItem {
    std::string displayLabel;
    std::string selectableId;
};

inline OutputDeviceComboItem MakeOutputDeviceComboItem(
    int index,
    const std::string& name,
    bool isDefault,
    const char* defaultSuffix)
{
    OutputDeviceComboItem item;
    item.displayLabel = name;
    if (isDefault && defaultSuffix != nullptr && defaultSuffix[0] != '\0') {
        item.displayLabel += defaultSuffix;
    }
    item.selectableId = item.displayLabel + "##output-device-" + std::to_string(index);
    return item;
}
