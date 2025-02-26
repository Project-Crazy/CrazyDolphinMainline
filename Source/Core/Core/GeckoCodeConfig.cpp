// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/GeckoCodeConfig.h"

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "Common/HttpRequest.h"
#include "Common/IniFile.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/CheatCodes.h"

namespace Gecko
{
std::vector<GeckoCode> LoadCodes(const Common::IniFile& globalIni, const Common::IniFile& localIni)
{
  std::vector<GeckoCode> gcodes;

  for (const auto* ini : {&globalIni, &localIni})
  {
    std::vector<std::string> lines;
    ini->GetLines("Gecko", &lines, false);

    GeckoCode gcode;

    std::erase_if(lines, [](const auto& line) { return line.empty() || line[0] == '#'; });

    for (auto& line : lines)
    {
      std::istringstream ss(line);

      // Some locales (e.g. fr_FR.UTF-8) don't split the string stream on space
      // Use the C locale to workaround this behavior
      ss.imbue(std::locale::classic());

      switch ((line)[0])
      {
      // enabled or disabled code
      case '+':
        ss.seekg(1);
        [[fallthrough]];
      case '$':
        if (!gcode.name.empty())
          gcodes.push_back(gcode);
        gcode = GeckoCode();
        gcode.enabled = (1 == ss.tellg());  // silly
        gcode.user_defined = (ini == &localIni);
        ss.seekg(1, std::ios_base::cur);
        // read the code name
        std::getline(ss, gcode.name, '[');  // stop at [ character (beginning of contributor name)
        gcode.name = StripWhitespace(gcode.name);
        // read the code creator name
        std::getline(ss, gcode.creator, ']');
        break;

      // notes
      case '*':
        gcode.notes.push_back(std::string(++line.begin(), line.end()));
        break;

      // either part of the code, or an option choice
      default:
      {
        GeckoCode::Code new_code;
        // TODO: support options
        if (std::optional<GeckoCode::Code> code = DeserializeLine(line))
          new_code = *code;
        else
          new_code.original_line = line;
        gcode.codes.push_back(new_code);
      }
      break;
      }
    }

    // add the last code
    if (!gcode.name.empty())
    {
      gcodes.push_back(gcode);
    }

    ReadEnabledAndDisabled(*ini, "Gecko", &gcodes);

    if (ini == &globalIni)
    {
      for (GeckoCode& code : gcodes)
        code.default_enabled = code.enabled;
    }
  }

  return gcodes;
}

static std::string MakeGeckoCodeTitle(const GeckoCode& code)
{
  std::string title = '$' + code.name;

  if (!code.creator.empty())
  {
    title += " [" + code.creator + ']';
  }

  return title;
}

// used by the SaveGeckoCodes function
static void SaveGeckoCode(std::vector<std::string>& lines, const GeckoCode& gcode)
{
  if (!gcode.user_defined)
    return;

  lines.push_back(MakeGeckoCodeTitle(gcode));

  // save all the code lines
  for (const GeckoCode::Code& code : gcode.codes)
  {
    lines.push_back(code.original_line);
  }

  // save the notes
  for (const std::string& note : gcode.notes)
    lines.push_back('*' + note);
}

void SaveCodes(Common::IniFile& inifile, const std::vector<GeckoCode>& gcodes)
{
  std::vector<std::string> lines;
  std::vector<std::string> enabled_lines;
  std::vector<std::string> disabled_lines;

  for (const GeckoCode& geckoCode : gcodes)
  {
    if (geckoCode.enabled != geckoCode.default_enabled)
      (geckoCode.enabled ? enabled_lines : disabled_lines).emplace_back('$' + geckoCode.name);

    SaveGeckoCode(lines, geckoCode);
  }

  inifile.SetLines("Gecko", lines);
  inifile.SetLines("Gecko_Enabled", enabled_lines);
  inifile.SetLines("Gecko_Disabled", disabled_lines);
}

std::optional<GeckoCode::Code> DeserializeLine(const std::string& line)
{
  std::vector<std::string> items = SplitString(line, ' ');

  GeckoCode::Code code;
  code.original_line = line;

  if (items.size() < 2)
    return std::nullopt;

  if (!TryParse(items[0], &code.address, 16))
    return std::nullopt;
  if (!TryParse(items[1], &code.data, 16))
    return std::nullopt;

  return code;
}

}  // namespace Gecko
