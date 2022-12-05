// internal
#include "poac/data/lockfile.hpp"

#include "poac/config.hpp"

namespace poac::data::lockfile::inline v1 {

// -------------------- INTO LOCKFILE --------------------

[[nodiscard]] auto
convert_to_lock(const resolver::UniqDeps<resolver::WithDeps>& deps)
    -> Result<toml::basic_value<toml::preserve_comments>> {
  Vec<Package> packages;
  for (const auto& [pack, inner_deps] : deps) {
    Package p{
        pack.name,
        pack.version_rq,
        Vec<String>{},
    };
    if (inner_deps.has_value()) {
      // Extract name from inner dependencies and drop version.
      Vec<String> ideps;
      for (const auto& [name, _v] : inner_deps.value()) {
        static_cast<void>(_v);
        ideps.emplace_back(name);
      }
      p.dependencies = ideps;
    }
    packages.emplace_back(p);
  }

  toml::basic_value<toml::preserve_comments> lock(
      Lockfile{.package = packages}, {String(lockfile_header)}
  );
  return Ok(lock);
}

[[nodiscard]] auto overwrite(const resolver::UniqDeps<resolver::WithDeps>& deps)
    -> Result<void> {
  const auto lock = Try(convert_to_lock(deps));
  std::ofstream lockfile(config::path::cwd / lockfile_name, std::ios::out);
  lockfile << lock;
  return Ok();
}

[[nodiscard]] auto generate(const resolver::UniqDeps<resolver::WithDeps>& deps)
    -> Result<void> {
  if (is_outdated(config::path::cwd)) {
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
  if (!fs::exists(base_dir / lockfile_name)) {
    return Ok(None);
  }

  try {
    const toml::value lock = toml::parse(base_dir / lockfile_name);
    const Lockfile parsed_lock = toml::get<Lockfile>(lock);
    if (parsed_lock.version != lockfile_version) {
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
