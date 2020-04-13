from conans import CMake, ConanFile, tools


class SboPtr(ConanFile):
    name = "sboptr"
    version = "0.1"
    revision_mode = "scm"
    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake_paths"
    exports_sources = (
        "include/*",
        "tests/*",
        "CMakeLists.txt",
    )
    build_requires = (
        "Catch2/2.11.1@catchorg/stable",
    )

    def build(self):
        cmake = CMake(self)

        cmake.configure()
        cmake.build()
        
        if tools.get_env("CONAN_RUN_TESTS", True):
            cmake.test()

    def package(self):
        self.copy("*.hpp", dst="include", src="include")

    def package_id(self):
        self.info.header_only()
