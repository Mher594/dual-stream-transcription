from conan import ConanFile
from conan.tools.cmake import cmake_layout


class KrispDesktopConan(ConanFile):
    name = "krisp_desktop"
    version = "1.0.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    default_options = {
        "qt/*:shared": True,
        "qt/*:openssl": True,
        "qt/*:gui": True,
        "qt/*:widgets": True,
        "qt/*:qtwebsockets": True,
        "qt/*:with_sqlite3": False,
        "qt/*:with_pq": False,
        "qt/*:with_odbc": False,
        "qt/*:with_mysql": False,
        "qt/*:qtdeclarative": False,
        "qt/*:qttools": False,
        "qt/*:qttranslations": False,
        "qt/*:qtwebengine": False,
        "qt/*:qtmultimedia": False,
        "qt/*:qtcharts": False,
        "qt/*:qtshadertools": False,
        "qt/*:qt5compat": False,
    }

    def requirements(self):
        self.requires("qt/6.8.3")
        self.requires("gtest/1.17.0")

    def layout(self):
        cmake_layout(self)
