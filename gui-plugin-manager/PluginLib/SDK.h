#pragma once

// SDK Wrapper - Exposes Unreal Engine SDK to plugins
// This header provides access to the Unreal Engine SDK from plugin_manager_base

// Include the main SDK header
#include "../../plugin_manager_base/Plugin_Manager_Base_SDK/SDK/SDK.hpp"

namespace PluginSDK {
    // Convenience aliases for common SDK types
    using FVector = SDK::FVector;
    using FRotator = SDK::FRotator;
    using FQuat = SDK::FQuat;
    using FLinearColor = SDK::FLinearColor;
    using FString = SDK::FString;
    using FName = SDK::FName;
    using FText = SDK::FText;

    // Common Unreal classes
    using UObject = SDK::UObject;
    using UClass = SDK::UClass;
    using UFunction = SDK::UFunction;
    using UProperty = SDK::UProperty;
    using UField = SDK::UField;
    using UStruct = SDK::UStruct;
    using ACharacter = SDK::ACharacter;
    using APawn = SDK::APawn;
    using AController = SDK::AController;
    using APlayerController = SDK::APlayerController;
    using UWorld = SDK::UWorld;
    using UGameplayStatics = SDK::UGameplayStatics;
    using ULocalPlayer = SDK::ULocalPlayer;
    using UCheatManager = SDK::UCheatManager;
    using AHUD = SDK::AHUD;
    using AOakCharacter = SDK::AOakCharacter;

    // Enum types
    using EObjectFlags = SDK::EObjectFlags;
    using EFunctionFlags = SDK::EFunctionFlags;

    // Helper functions for common operations
    inline UWorld* GetWorld() {
        return SDK::UWorld::GetWorld();
    }

    // Gets the default object of a class
    template<typename T>
    inline T* GetDefaultObject() {
        return T::GetDefaultObj();
    }

    // Finds an object by name
    inline UObject* FindObject(const char* Name) {
        return SDK::UObject::FindObject(Name);
    }
}
