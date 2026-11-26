#pragma once

// Semver：与 git tag 一致（tag 为 vX.Y.Z → 此处 X.Y.Z）。显示：0.1.7(152) 其中括号为 APP_BUILD。
static constexpr const char *APP_VERSION = "0.1.7";
static constexpr int APP_BUILD = 153;
static constexpr const char *APP_UPDATE_TIME = __DATE__ " " __TIME__;
static constexpr const char *APP_AUTHOR = "KyleBing";
