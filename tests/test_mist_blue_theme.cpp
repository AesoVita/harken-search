#include "AppState.h"
#include "Theme.h"
#include "BuiltinThemeScheme.h"
#include "ui/TransportContrast.h"
#include "ui/MistBlueChrome.h"
#include <cstdio>
int main() {
    using namespace TransportContrast;
    auto s=MakeBuiltinScheme(AppTheme::LapisBlue);
    int failures=0;
    auto check=[&](bool ok,const char* name){std::printf("%s %s\n",ok?"PASS":"FAIL",name);failures+=!ok;};
    check(s.filterBrowserBG.x>.98f && s.filterBrowserBG.y>.98f && s.filterBrowserBG.z>.98f,"white filter panel");
    for(auto bg:{s.resultsBG,s.toolbarBG,s.metadataPanelBG,s.waveformBG,s.transportBarBG}) {
        check(Luminance(bg)>.035 && Luminance(bg)<.13 && bg.z-bg.x>.08,"navy major surface close to original");
        check(Ratio(s.interfaceText,bg)>=4.5,"readable shared text on blue surfaces");
        check(Ratio(bg,s.resultsBG)<1.6,"gentle panel-to-panel contrast");
    }
    check(s.waveformColor.x>.98f && s.waveformColor.y>.98f && s.waveformColor.z>.98f,"white waveform line");
    check(Ratio(s.waveformColor,s.waveformBG)>=4.5,"white waveform contrast");
    check(Ratio(s.filterBrowserText,s.filterBrowserBG)>=4.5,"filter text");
    check(Ratio(s.filterBrowserHeading,s.sidebarHeaderBG)>=4.5,"filter heading");
    check(Ratio(s.metadataPanelLabel,s.metadataPanelBG)>=4.5,"metadata labels");
    check(Ratio(s.resultsSecondaryText,s.resultsBG)>=4.5,"secondary result labels");
    ImVec4 colors[ImGuiCol_COUNT]{};
    MistBlueChrome::Apply(colors,s);
    for(auto role:{ImGuiCol_ButtonActive,ImGuiCol_TabActive,ImGuiCol_TabHovered,ImGuiCol_HeaderActive})
        check(Ratio(s.interfaceText,colors[role])>=4.5,"active button and tab text");
    auto tabInk=MistBlueChrome::ResultsTabText(s);
    check(Ratio(tabInk,s.resultsTabActive)>=4.5,"selected result tab text");
    return failures?1:0;
}
