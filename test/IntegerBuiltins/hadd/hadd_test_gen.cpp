// Copyright 2020 The Clspv Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

const std::string preamble = R"(
; RUN: clspv-opt -ReplaceOpenCLBuiltin %s -o %t.ll
; RUN: FileCheck %s < %t.ll

; AUTO-GENERATED TEST FILE
; This test was generated by add_sat_test_gen.cpp.
; Please modify the that file and regenerate the tests to make changes.

target datalayout = "e-p:32:32-i64:64-v16:16-v24:32-v32:32-v48:64-v96:128-v192:256-v256:256-v512:512-v1024:1024"
target triple = "spir-unknown-unknown"
)";

std::string TypeName(uint32_t width, bool is_signed, uint32_t vector) {
  std::string name = (is_signed ? "" : "u");
  switch (width) {
  case 8:
    name += "char";
    break;
  case 16:
    name += "short";
    break;
  case 32:
    name += "int";
    break;
  case 64:
  default:
    name += "long";
    break;
  }

  if (vector > 1)
    name += std::to_string(vector);

  return name;
}

std::string Params(uint32_t width, bool is_signed, uint32_t vector,
                   uint32_t params) {
  std::string base;
  switch (width) {
  case 8:
    base = is_signed ? "c" : "h";
    break;
  case 16:
    base = is_signed ? "s" : "t";
    break;
  case 32:
    base = is_signed ? "i" : "j";
    break;
  case 64:
  default:
    base = is_signed ? "l" : "m";
    break;
  }

  if (vector == 1) {
    if (params == 1)
      return base;
    else if (params == 2)
      return base + base;
    else
      return base + base + base;
  }

  if (params == 1)
    return "Dv" + std::to_string(vector) + "_" + base;
  else if (params == 2)
    return "Dv" + std::to_string(vector) + "_" + base + "S_";
  else
    return "Dv" + std::to_string(vector) + "_" + base + "S_S_";
}

std::string LLVMTypeName(uint32_t width, uint32_t vector) {
  std::string base_type = "i" + std::to_string(width);
  if (vector == 1)
    return base_type;

  return "<" + std::to_string(vector) + " x " + base_type + ">";
}

std::string SplatConstant(uint32_t vector, const std::string &type,
                          const std::string &value) {
  if (vector == 1)
    return value;

  std::string constant = "<";
  for (auto i = 0; i < vector; ++i) {
    constant += type + " " + value;
    constant += (i == (vector - 1) ? "" : ", ");
  }
  constant += ">";
  return constant;
}

int main() {

  std::vector<uint32_t> widths = {8, 16, 32, 64};
  std::vector<uint32_t> sizes = {1, 2, 3, 4};

  for (auto func : {"hadd", "rhadd"}) {
    for (auto width : widths) {
      for (auto is_signed : {false, true}) {
        for (auto size : sizes) {
          const std::string c_name = TypeName(width, is_signed, size);
          const std::string llvm_name = LLVMTypeName(width, size);
          const std::string llvm_func =
              (strcmp(func, "hadd") == 0 ? "_Z4hadd" : "_Z5rhadd");
          const std::string join_op =
              (strcmp(func, "hadd") == 0 ? "and" : "or");

          std::ofstream str(std::string(func) + "_" + c_name + ".ll");
          str << preamble << "\n";

          str << "define " << llvm_name << " @" << func << "_" << c_name << "("
              << llvm_name << " %a, " << llvm_name << " %b) {\n";
          str << "entry:\n";
          str << " %call = call " << llvm_name << " @" << llvm_func
              << Params(width, is_signed, size, 2) << "(" << llvm_name
              << " %a, " << llvm_name << " %b)\n";
          str << " ret " << llvm_name << " %call\n";
          str << "}\n\n";
          str << "declare " << llvm_name << " @" << llvm_func
              << Params(width, is_signed, size, 2) << "(" << llvm_name << ", "
              << llvm_name << ")\n";
          str << "\n";

          std::string shift_ins = is_signed ? "ashr" : "lshr";
          str << "; CHECK: [[a_shr:%[a-zA_Z0-9_.]+]] = " << shift_ins << " "
              << llvm_name << " %a, "
              << SplatConstant(size, LLVMTypeName(width, 1), "1") << "\n";
          str << "; CHECK: [[b_shr:%[a-zA-Z0-9_.]+]] = " << shift_ins << " "
              << llvm_name << " %b, "
              << SplatConstant(size, LLVMTypeName(width, 1), "1") << "\n";
          str << "; CHECK: [[add:%[a-zA-Z0-9_.]+]] = add " << llvm_name
              << " [[a_shr]], [[b_shr]]\n";
          str << "; CHECK: [[join:%[a-zA-Z0-9_.]+]] = " << join_op << " "
              << llvm_name << " %a, %b\n";
          str << "; CHECK: [[and:%[a-zA-Z0-9_.]+]] = and " << llvm_name
              << " [[join]], "
              << SplatConstant(size, LLVMTypeName(width, 1), "1") << "\n";
          str << "; CHECK: [[hadd:%[a-zA-Z0-9_.]+]] = add " << llvm_name
              << " [[add]], [[and]]\n";
          str << "; CHECK: ret " << llvm_name << " [[hadd]]\n";

          str.close();
        }
      }
    }
  }

  return 0;
}
