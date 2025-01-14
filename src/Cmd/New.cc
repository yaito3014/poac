#include "New.hpp"

#include "../Algos.hpp"
#include "../Cli.hpp"
#include "../Git2.hpp"
#include "../Logger.hpp"
#include "../Manifest.hpp"
#include "../Rustify/Result.hpp"
#include "Common.hpp"

#include <cstdlib>
#include <fstream>
#include <span>
#include <string>
#include <string_view>

namespace cabin {

static Result<void> newMain(std::span<const std::string_view> args);

const Subcmd NEW_CMD =  //
    Subcmd{ "new" }
        .setDesc("Create a new cabin project")
        .addOpt(OPT_BIN)
        .addOpt(OPT_LIB)
        .setArg(Arg{ "name" })
        .setMainFn(newMain);

static constexpr std::string_view MAIN_CC =
    "#include <iostream>\n\n"
    "int main() {\n"
    "  std::cout << \"Hello, world!\" << std::endl;\n"
    "  return 0;\n"
    "}\n";

static std::string
getAuthor() noexcept {
  try {
    git2::Config config = git2::Config();
    config.openDefault();
    return config.getString("user.name") + " <" + config.getString("user.email")
           + ">";
  } catch (const git2::Exception& e) {
    logger::debug("{}", e.what());
    return "";
  }
}

std::string
createCabinToml(const std::string_view projectName) noexcept {
  std::string cabinToml =
      "[package]\n"
      "name = \"";
  cabinToml += projectName;
  cabinToml +=
      "\"\n"
      "version = \"0.1.0\"\n"
      "authors = [\"";
  cabinToml += getAuthor();
  cabinToml +=
      "\"]\n"
      "edition = \"20\"\n";
  return cabinToml;
}

static std::string
getHeader(const std::string_view projectName) noexcept {
  const std::string projectNameUpper = toMacroName(projectName);
  std::string header = "#ifndef " + projectNameUpper + "_HPP\n"
                  "#define " + projectNameUpper + "_HPP\n\n"
                  "namespace ";
  header += projectName;
  header +=
      " {\n}\n\n"
      "#endif  // !"
      + projectNameUpper + "_HPP\n";
  return header;
}

static Result<void>
writeToFile(
    std::ofstream& ofs, const fs::path& fpath, const std::string_view text
) {
  ofs.open(fpath);
  if (ofs.is_open()) {
    ofs << text;
  }
  ofs.close();

  if (!ofs) {
    Bail("writing `{}` failed", fpath.string());
  }
  ofs.clear();
  return Ok();
}

static Result<void>
createTemplateFiles(const bool isBin, const std::string_view projectName) {
  std::ofstream ofs;

  if (isBin) {
    fs::create_directories(projectName / "src"_path);
    Try(writeToFile(
        ofs, projectName / "cabin.toml"_path, createCabinToml(projectName)
    ));
    Try(writeToFile(ofs, projectName / ".gitignore"_path, "/cabin-out"));
    Try(writeToFile(ofs, projectName / "src"_path / "main.cc", MAIN_CC));

    logger::info("Created", "binary (application) `{}` package", projectName);
  } else {
    fs::create_directories(projectName / "include"_path / projectName);
    Try(writeToFile(
        ofs, projectName / "cabin.toml"_path, createCabinToml(projectName)
    ));
    Try(writeToFile(
        ofs, projectName / ".gitignore"_path, "/cabin-out\ncabin.lock"
    ));
    Try(writeToFile(
        ofs,
        (projectName / "include"_path / projectName / projectName).string()
            + ".hpp",
        getHeader(projectName)
    ));

    logger::info("Created", "library `{}` package", projectName);
  }
  return Ok();
}

static Result<void>
newMain(const std::span<const std::string_view> args) {
  // Parse args
  bool isBin = true;
  std::string packageName;
  for (auto itr = args.begin(); itr != args.end(); ++itr) {
    const auto control = Try(Cli::handleGlobalOpts(itr, args.end(), "new"));
    if (control == Cli::Return) {
      return Ok();
    } else if (control == Cli::Continue) {
      continue;
    } else if (*itr == "-b" || *itr == "--bin") {
      isBin = true;
    } else if (*itr == "-l" || *itr == "--lib") {
      isBin = false;
    } else if (packageName.empty()) {
      packageName = *itr;
    } else {
      return NEW_CMD.noSuchArg(*itr);
    }
  }

  Try(validatePackageName(packageName));
  Ensure(
      !fs::exists(packageName), "directory `{}` already exists", packageName
  );

  Try(createTemplateFiles(isBin, packageName));
  git2::Repository().init(packageName);
  return Ok();
}

}  // namespace cabin
