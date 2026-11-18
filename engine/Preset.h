// Nacre — preset text format.
//
//   @name=Glass Horizon
//   @category=Pads
//   @tags=warm,evolving,wide
//   @author=Nacre Factory
//   oscA_table=Vowels         <- choices are stored by NAME, so tables can be
//   filter1_cutoff=1200          appended later without breaking old presets
//
// Only values that differ from the defaults are written. Unknown ids are ignored
// (forward compatibility); missing ids take their default (backward compatibility).
#pragma once
#include <map>
#include <sstream>
#include "Params.h"

namespace nacre {

struct Preset {
    std::string name = "Init", category = "Init", tags, author = "Nacre";
    float values[kNumParams];
    Preset() { for (int i = 0; i < kNumParams; ++i) values[i] = kParams[i].def; }

    static std::string valueToText(const ParamDef& d, float v) {
        if (d.type == kChoice) return splitChoices(d.choices)[(size_t) std::clamp((int) std::lround(v), 0, (int) d.hi)];
        char b[32];
        if (d.type != kFloat) snprintf(b, sizeof b, "%d", (int) std::lround(v));
        else snprintf(b, sizeof b, "%.5g", v);
        return b;
    }
    static float textToValue(const ParamDef& d, const std::string& s) {
        if (d.type == kChoice) {
            auto c = splitChoices(d.choices);
            for (size_t i = 0; i < c.size(); ++i) if (c[i] == s) return (float) i;
            return std::clamp((float) std::atof(s.c_str()), d.lo, d.hi);   // numeric fallback
        }
        return snapValue(d, (float) std::atof(s.c_str()));
    }

    std::string serialize() const {
        std::ostringstream o;
        o << "@name=" << name << "\n@category=" << category << "\n";
        if (!tags.empty()) o << "@tags=" << tags << "\n";
        o << "@author=" << author << "\n";
        for (int i = 0; i < kNumParams; ++i)
            if (std::fabs(values[i] - kParams[i].def) > 1e-6f) o << kParams[i].id << "=" << valueToText(kParams[i], values[i]) << "\n";
        return o.str();
    }

    static Preset parse(const std::string& text) {
        Preset p;
        std::istringstream in(text);
        std::string line;
        while (std::getline(in, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            auto eq = line.find('=');
            if (line.empty() || line[0] == '#' || eq == std::string::npos) continue;
            std::string k = line.substr(0, eq), v = line.substr(eq + 1);
            if (k == "@name") p.name = v;
            else if (k == "@category") p.category = v;
            else if (k == "@tags") p.tags = v;
            else if (k == "@author") p.author = v;
            else { int id = findParam(k); if (id >= 0) p.values[id] = textToValue(kParams[id], v); }
        }
        return p;
    }
};

struct FactoryPreset { const char* name; const char* category; const char* tags; const char* data; };
const FactoryPreset* factoryPresets(int& count);

} // namespace nacre
