plugins {
    id("com.android.application")
}

android {
    namespace = "com.killdano.licenserevoked"
    compileSdk = 36
    ndkVersion = "27.2.12479018"

    defaultConfig {
        applicationId = "com.killdano.licenserevoked"
        minSdk = 30
        targetSdk = 36
        versionCode = 1
        versionName = "5.17"
        ndk { abiFilters += listOf("arm64-v8a") }
        externalNativeBuild { cmake { arguments += listOf("-DANDROID_STL=none") } }
    }
    externalNativeBuild {
        cmake { path = file("src/main/cpp/CMakeLists.txt"); version = "3.22.1" }
    }
    buildTypes {
        release { isMinifyEnabled = false }
    }
}
