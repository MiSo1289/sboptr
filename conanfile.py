from conans import CMake, ConanFile, tools


class SboPtr(ConanFile):
    name = "sboptr"
    version = "0.2.0"
    url = "https://github.com/MiSo1289/sboptr"
    license = "MIT"
    description = "Smart pointer type with configurable small buffer storage"
    revision_mode = "scm"
    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake_paths"
    exports_sources = (
        "examples/*",
        "include/*",
        "tests/*",
        "CMakeLists.txt",
    )
    build_requires = (
        "Catch2/2.11.1@catchorg/stable",
    )

    def build(self):
        cmake = CMake(self)

        cmake.definitions["CMAKE_TOOLCHAIN_FILE"] = "conan_paths.cmake"
        cmake.configure()
        cmake.build(target="sboptr_tests")
        
        if tools.get_env("CONAN_RUN_TESTS", True):
            cmake.test()

    def package(self):
        self.copy("*.hpp", dst="include", src="include")

    def package_id(self):
        self.info.header_only()
