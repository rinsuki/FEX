#include <FEXCore/Config/Config.h>
#include <FEXCore/Utils/File.h>
#include <FEXCore/Utils/LogManager.h>
#include <FEXCore/Utils/Telemetry.h>
#include <FEXCore/fextl/fmt.h>
#include <FEXCore/fextl/string.h>
#include <FEXHeaderUtils/Filesystem.h>

#include <array>
#include <stddef.h>
#include <string_view>
#include <system_error>

namespace FEXCore::Telemetry {
#ifndef FEX_DISABLE_TELEMETRY
  static std::array<Value, FEXCore::Telemetry::TelemetryType::TYPE_LAST> TelemetryValues = {{ }};
  const std::array<std::string_view, FEXCore::Telemetry::TelemetryType::TYPE_LAST> TelemetryNames {
    "64byte Split Locks",
    "16byte Split atomics",
    "VEX instructions (AVX)",
    "EVEX instructions (AVX512)",
    "16bit CAS Tear",
    "32bit CAS Tear",
    "64bit CAS Tear",
    "128bit CAS Tear",
    "Crash mask",
  };
  void Initialize() {
    auto DataDirectory = Config::GetDataDirectory();
    DataDirectory += "Telemetry/";

    // Ensure the folder structure is created for our configuration
    if (!FHU::Filesystem::Exists(DataDirectory) &&
        !FHU::Filesystem::CreateDirectories(DataDirectory)) {
      LogMan::Msg::IFmt("Couldn't create telemetry Folder");
    }
  }

  void Shutdown(fextl::string const &ApplicationName) {
    auto DataDirectory = Config::GetDataDirectory();
    DataDirectory += "Telemetry/" + ApplicationName + ".telem";

    if (FHU::Filesystem::Exists(DataDirectory)) {
      // If the file exists, retain a single backup
      auto Backup = DataDirectory + ".1";
      FHU::Filesystem::CopyFile(DataDirectory, Backup, FHU::Filesystem::CopyOptions::OVERWRITE_EXISTING);
    }

    auto File = FEXCore::File::File(DataDirectory.c_str(),
         FEXCore::File::FileModes::WRITE |
         FEXCore::File::FileModes::CREATE |
         FEXCore::File::FileModes::TRUNCATE);

    if (File.IsValid()) {
      for (size_t i = 0; i < TelemetryType::TYPE_LAST; ++i) {
        auto &Name = TelemetryNames.at(i);
        auto &Data = TelemetryValues.at(i);
        fextl::fmt::print(File, "{}: {}\n", Name, *Data);
      }
      File.Flush();
    }
  }

  Value &GetObject(TelemetryType Type) {
    return TelemetryValues.at(Type);
  }
#endif
}
