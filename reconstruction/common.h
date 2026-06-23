#pragma once

#ifndef _WIN32_DCOM
#define _WIN32_DCOM
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <comcat.h>
#include <objbase.h>
#include <ocidl.h>
#include <oleauto.h>

#include <OpcEnum.h>
#include <opccomn.h>
#include <opcda.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace opcclient {

void InitializeUtf8Io();

template <typename T>
class ComPtr {
public:
    ComPtr() = default;
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    ComPtr(ComPtr&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }

    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            Reset();
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    ~ComPtr() {
        Reset();
    }

    T* Get() const {
        return ptr_;
    }

    T** Put() {
        Reset();
        return &ptr_;
    }

    T* operator->() const {
        return ptr_;
    }

    explicit operator bool() const {
        return ptr_ != nullptr;
    }

    void Attach(T* value) {
        Reset();
        ptr_ = value;
    }

    T* Detach() {
        T* value = ptr_;
        ptr_ = nullptr;
        return value;
    }

    void CopyFrom(T* value) {
        Reset();
        ptr_ = value;
        if (ptr_) {
            ptr_->AddRef();
        }
    }

    void Reset() {
        if (ptr_) {
            ptr_->Release();
            ptr_ = nullptr;
        }
    }

    template <typename U>
    HRESULT As(REFIID iid, ComPtr<U>& output) const {
        output.Reset();
        if (!ptr_) {
            return E_POINTER;
        }
        return ptr_->QueryInterface(iid, reinterpret_cast<void**>(output.Put()));
    }

private:
    T* ptr_ = nullptr;
};

std::wstring HResultText(HRESULT hr);
std::string Narrow(const std::wstring& value);
std::wstring WidenUtf8(const std::string& value);
std::wstring Trim(std::wstring value);
void Check(HRESULT hr, const wchar_t* operation);
std::wstring GuidText(REFGUID guid);
std::wstring FileTimeText(const FILETIME& fileTime);
std::wstring VariantTypeText(VARTYPE type);
std::wstring VariantText(const VARIANT& value);
std::wstring QualityText(WORD quality);
std::wstring ServerStateText(OPCSERVERSTATE state);

} // namespace opcclient
