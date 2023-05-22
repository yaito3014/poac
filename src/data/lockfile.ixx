module;

// std
#include <filesystem>
#include <fstream>
#include <utility>

// external
#include <toml.hpp>

// internal
#include "../util/result-macros.hpp"

export module poac.data.lockfile;

import poac.config;
import poac.core.resolver.types;
import poac.data.manifest;
import poac.util.result;
import poac.util.rustify;

export namespace poac::data::lockfile {

inline constexpr StringRef LOCKFILE_NAME = "poac.lock";
inline constexpr StringRef LOCKFILE_HEADER =
    " This file is automatically generated by Poac.\n"
    "# It is not intended for manual editing.";

using InvalidLockfileVersion = Error<"invalid lockfile version found: {}", i64>;
using FailedToReadLockfile = Error<"failed to read lockfile:\n{}", String>;

inline auto poac_lock_last_modified(const Path& base_dir)
    -> fs::file_time_type {
  return fs::last_write_time(base_dir / LOCKFILE_NAME);
}

inline auto is_outdated(const Path& base_dir) -> bool {
  if (!fs::exists(base_dir / LOCKFILE_NAME)) {
    return true;
  }
  return poac_lock_last_modified(base_dir)
         < manifest::poac_toml_last_modified(base_dir);
}

} // namespace poac::data::lockfile

namespace poac::data::lockfile::inline v1 {

namespace resolver = core::resolver::resolve;

inline constexpr i64 LOCKFILE_VERSION = 1;

// NOLINTNEXTLINE(bugprone-exception-escape)
struct Package {
  String name;
  String version;
  Vec<String> dependencies;
};

struct Lockfile {
  i64 version = LOCKFILE_VERSION;
  Vec<Package> package;
};

// clang-format off
// to avoid reporting errors with inline namespace on only the dry-run mode. (IDK why)
} // namespace poac::data::lockfile::v1
// clang-format on

// NOLINTNEXTLINE(modernize-use-trailing-return-type)
TOML11_DEFINE_CONVERSION_NON_INTRUSIVE(
    poac::data::lockfile::v1::Package, name, version, dependencies
)
// NOLINTNEXTLINE(modernize-use-trailing-return-type)
TOML11_DEFINE_CONVERSION_NON_INTRUSIVE(
    poac::data::lockfile::v1::Lockfile, version, package
)

export namespace poac::data::lockfile::inline v1 {

// -------------------- INTO LOCKFILE --------------------

[[nodiscard]] auto
convert_to_lock(const resolver::UniqDeps<resolver::WithDeps>& deps)
    -> Result<toml::basic_value<toml::preserve_comments>> {
  Vec<Package> packages;
  for (const auto& [ pack, inner_deps ] : deps) {
    if (pack.dep_info.type != "poac") {
      continue;
    }
    Package p{
        pack.name,
        pack.dep_info.version_rq,
        Vec<String>{},
    };
    if (inner_deps.has_value()) {
      // Extract name from inner dependencies and drop version.
      Vec<String> ideps;
      for (const auto& [ name, _v ] : inner_deps.value()) {
        static_cast<void>(_v);
        ideps.emplace_back(name);
      }
      p.dependencies = ideps;
    }
    packages.emplace_back(p);
  }

  toml::basic_value<toml::preserve_comments> lock(
      Lockfile{.package = packages}, {String(LOCKFILE_HEADER)}
  );
  return Ok(lock);
}

[[nodiscard]] auto overwrite(const resolver::UniqDeps<resolver::WithDeps>& deps)
    -> Result<void> {
  const auto lock = Try(convert_to_lock(deps));
  std::ofstream lockfile(config::cwd / LOCKFILE_NAME, std::ios::out);
  lockfile << lock;
  return Ok();
}

[[nodiscard]] auto generate(const resolver::UniqDeps<resolver::WithDeps>& deps)
    -> Result<void> {
  if (is_outdated(config::cwd)) {
    return overwrite(deps);
  }
  return Ok();
}

// -------------------- FROM LOCKFILE --------------------

[[nodiscard]] auto convert_to_deps(const Lockfile& lock)
    -> resolver::UniqDeps<resolver::WithDeps> {
  resolver::UniqDeps<resolver::WithDeps> deps;
  for (const auto& package : lock.package) {
    resolver::UniqDeps<resolver::WithDeps>::mapped_type inner_deps = None;
    if (!package.dependencies.empty()) {
      // When serializing lockfile, package version of inner dependencies
      // will be dropped (ref: `convert_to_lock` function).
      // Thus, the version should be restored just as empty string ("").
      resolver::UniqDeps<resolver::WithDeps>::mapped_type::value_type ideps;
      for (const auto& name : package.dependencies) {
        ideps.push_back({name, ""});
      }
      inner_deps = ideps;
    }
    deps.emplace(resolver::Package{package.name, package.version}, inner_deps);
  }
  return deps;
}

[[nodiscard]] auto read(const Path& base_dir)
    -> Result<Option<resolver::UniqDeps<resolver::WithDeps>>> {
  if (!fs::exists(base_dir / LOCKFILE_NAME)) {
    return Ok(None);
  }

  try {
    const toml::value lock = toml::parse(base_dir / LOCKFILE_NAME);
    const Lockfile parsed_lock = toml::get<Lockfile>(lock);
    if (parsed_lock.version != LOCKFILE_VERSION) {
      return Err<InvalidLockfileVersion>(parsed_lock.version);
    }
    return Ok(convert_to_deps(parsed_lock));
  } catch (const std::exception& e) {
    return Err<FailedToReadLockfile>(e.what());
  }
}

// clang-format off
// to avoid reporting errors with inline namespace on only the dry-run mode. (IDK why)
} // namespace poac::data::lockfile::v1
// clang-format on
