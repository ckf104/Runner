// Copyright 2023, Dumitru Zahvatov, All rights reserved.

#include "SimpleHealthBar.h"

#define LOCTEXT_NAMESPACE "FSimpleHealthBarModule"

void FSimpleHealthBarModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FSimpleHealthBarModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSimpleHealthBarModule, SimpleHealthBar)