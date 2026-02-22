#include "test_helper.h"

int main(void) {
    TEST_SUITE_BEGIN("Version API");

    /* Test yaml_get_version_string */
    {
        const char *version = yaml_get_version_string();
        ASSERT_NOT_NULL(version, "version string should not be null");
        ASSERT(strlen(version) > 0, "version string should not be empty");
        /* Should be in X.Y.Z format */
        int dots = 0;
        for (const char *p = version; *p; p++) {
            if (*p == '.') dots++;
        }
        ASSERT_EQ_INT(dots, 2, "version should have exactly 2 dots (X.Y.Z)");
    }

    /* Test yaml_get_version */
    {
        int major = -1, minor = -1, patch = -1;
        yaml_get_version(&major, &minor, &patch);
        ASSERT(major >= 0, "major version >= 0");
        ASSERT(minor >= 0, "minor version >= 0");
        ASSERT(patch >= 0, "patch version >= 0");
    }

    /* Version string should match version numbers */
    {
        int major, minor, patch;
        yaml_get_version(&major, &minor, &patch);
        const char *version = yaml_get_version_string();
        char expected[64];
        snprintf(expected, sizeof(expected), "%d.%d.%d", major, minor, patch);
        ASSERT_EQ_STR(version, expected, "version string should match version numbers");
    }

    TEST_SUITE_END();
}
