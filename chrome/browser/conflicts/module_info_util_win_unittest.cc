// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/module_info_util_win.h"

#include <windows.h>

#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/environment.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_environment_variable_override.h"
#include "base/win/pe_image.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Creates a truncated copy of the current executable at |location| path to
// mimic a module with an invalid NT header.
bool CreateTruncatedModule(const base::FilePath& location) {
  base::FilePath file_exe_path;
  if (!base::PathService::Get(base::FILE_EXE, &file_exe_path))
    return false;

  base::File file_exe(file_exe_path,
                      base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file_exe.IsValid())
    return false;

  const size_t kSizeOfTruncatedDll = 256;
  char buffer[kSizeOfTruncatedDll];
  if (file_exe.Read(0, buffer, kSizeOfTruncatedDll) != kSizeOfTruncatedDll)
    return false;

  base::File target_file(location,
                         base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  if (!target_file.IsValid())
    return false;

  return target_file.Write(0, buffer, kSizeOfTruncatedDll) ==
         kSizeOfTruncatedDll;
}

}  // namespace

TEST(ModuleInfoUtilTest, GetCertificateInfoUnsigned) {
  base::FilePath path;
  ASSERT_TRUE(base::PathService::Get(base::FILE_EXE, &path));
  CertificateInfo cert_info;
  GetCertificateInfo(path, &cert_info);
  EXPECT_EQ(CertificateType::NO_CERTIFICATE, cert_info.type);
  EXPECT_TRUE(cert_info.path.empty());
  EXPECT_TRUE(cert_info.subject.empty());
}

TEST(ModuleInfoUtilTest, GetCertificateInfoSigned) {
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  std::string sysroot;
  ASSERT_TRUE(env->GetVar("SYSTEMROOT", &sysroot));

  base::FilePath path =
      base::FilePath::FromUTF8Unsafe(sysroot).Append(L"system32\\kernel32.dll");

  CertificateInfo cert_info;
  GetCertificateInfo(path, &cert_info);
  EXPECT_NE(CertificateType::NO_CERTIFICATE, cert_info.type);
  EXPECT_FALSE(cert_info.path.empty());
  EXPECT_FALSE(cert_info.subject.empty());
}

TEST(ModuleInfoUtilTest, GetEnvironmentVariablesMapping) {
  base::test::ScopedEnvironmentVariableOverride scoped_override("foo",
                                                                "C:\\bar\\");

  // The mapping for these variables will be retrieved.
  std::vector<base::string16> environment_variables = {
      L"foo", L"SYSTEMROOT",
  };
  StringMapping string_mapping =
      GetEnvironmentVariablesMapping(environment_variables);

  ASSERT_EQ(2u, string_mapping.size());

  EXPECT_STREQ(L"c:\\bar", string_mapping[0].first.c_str());
  EXPECT_STREQ(L"%foo%", string_mapping[0].second.c_str());
  EXPECT_FALSE(string_mapping[1].second.empty());
}

const struct CollapsePathList {
  base::string16 expected_result;
  base::string16 test_case;
} kCollapsePathList[] = {
    // Negative testing (should not collapse this path).
    {L"c:\\a\\a.dll", L"c:\\a\\a.dll"},
    // These two are to test that we select the maximum collapsed path.
    {L"%foo%\\a.dll", L"c:\\foo\\a.dll"},
    {L"%x%\\a.dll", L"c:\\foo\\bar\\a.dll"},
    // Tests that only full path components are collapsed.
    {L"c:\\foo_bar\\a.dll", L"c:\\foo_bar\\a.dll"},
};

TEST(ModuleInfoUtilTest, CollapseMatchingPrefixInPath) {
  StringMapping string_mapping = {
      std::make_pair(L"c:\\foo", L"%foo%"),
      std::make_pair(L"c:\\foo\\bar", L"%x%"),
  };

  for (size_t i = 0; i < base::size(kCollapsePathList); ++i) {
    base::string16 test_case = kCollapsePathList[i].test_case;
    CollapseMatchingPrefixInPath(string_mapping, &test_case);
    EXPECT_EQ(kCollapsePathList[i].expected_result, test_case);
  }
}

// Tests that GetModuleImageSizeAndTimeDateStamp() returns the same information
// from a module that has been loaded in memory.
TEST(ModuleInfoUtilTest, GetModuleImageSizeAndTimeDateStamp) {
  // Use the current exe file as an arbitrary module that exists.
  base::FilePath file_exe;
  ASSERT_TRUE(base::PathService::Get(base::FILE_EXE, &file_exe));

  // Read the values from the loaded module.
  base::ScopedNativeLibrary scoped_loaded_module(file_exe);
  base::win::PEImage pe_image(scoped_loaded_module.get());
  IMAGE_NT_HEADERS* nt_headers = pe_image.GetNTHeaders();

  // Read the values from the module on disk.
  uint32_t size_of_image = 0;
  uint32_t time_date_stamp = 0;
  EXPECT_TRUE(GetModuleImageSizeAndTimeDateStamp(file_exe, &size_of_image,
                                                 &time_date_stamp));

  EXPECT_EQ(nt_headers->OptionalHeader.SizeOfImage, size_of_image);
  EXPECT_EQ(nt_headers->FileHeader.TimeDateStamp, time_date_stamp);
}

TEST(ModuleInfoUtilTest, NonexistentDll) {
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  base::FilePath nonexistant_dll =
      scoped_temp_dir.GetPath().Append(L"nonexistant.dll");

  uint32_t size_of_image = 0;
  uint32_t time_date_stamp = 0;
  EXPECT_FALSE(GetModuleImageSizeAndTimeDateStamp(
      nonexistant_dll, &size_of_image, &time_date_stamp));
}

TEST(ModuleInfoUtilTest, InvalidNTHeader) {
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  base::FilePath invalid_dll =
      scoped_temp_dir.GetPath().Append(L"truncated.dll");
  ASSERT_TRUE(CreateTruncatedModule(invalid_dll));

  uint32_t size_of_image = 0;
  uint32_t time_date_stamp = 0;
  EXPECT_FALSE(GetModuleImageSizeAndTimeDateStamp(invalid_dll, &size_of_image,
                                                  &time_date_stamp));
}

TEST(ModuleInfoUtilTest, NormalizeCertificateSubject) {
  base::string16 test_case = base::string16(L"signer\0", 7);
  EXPECT_EQ(7u, test_case.length());

  base::string16 expected = L"signer";

  internal::NormalizeCertificateSubject(&test_case);

  EXPECT_EQ(test_case, expected);
}
