TERMUX_SUBPKG_DESCRIPTION="OpenJDK 21 runtime module image"
TERMUX_SUBPKG_INCLUDE="lib/jvm/java-21-openjdk/lib/modules"

# The runtime image is split so each signed APT object stays below GitHub's
# 100 MiB per-file limit. openjdk-21 depends on this package.
