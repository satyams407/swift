// RUN: %empty-directory(%t)
// RUN: %target-swift-frontend -emit-module-path %t/resilient_struct.swiftmodule %S/../Inputs/resilient_struct.swift -enable-library-evolution
// RUN: %target-swift-frontend(mock-sdk: %clang-importer-sdk) -emit-module-path %t/resilient_objc_class.swiftmodule %S/../Inputs/resilient_objc_class.swift -I %t -enable-library-evolution -emit-objc-header-path %t/resilient_objc_class.h

// RUN: cp %S/Inputs/custom-modules/module.map %t/module.map

// RUN: %target-swift-frontend(mock-sdk: %clang-importer-sdk) -typecheck %S/resilient-ancestry.swift -module-name resilient -emit-objc-header-path %t/resilient.h -I %t -enable-library-evolution -enable-resilient-objc-class-stubs
// RUN: %FileCheck %S/resilient-ancestry.swift < %t/resilient.h
// RUN: %check-in-clang %t/resilient.h -I %t

// REQUIRES: objc_interop
// REQUIRES: swift_stable_abi
