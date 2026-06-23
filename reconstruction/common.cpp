#include "common.h"

#include <clocale>
#include <locale>

namespace opcclient {
namespace {

bool IsConsoleHandle(DWORD standardHandle) {
    HANDLE handle = GetStdHandle(standardHandle);
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD mode = 0;
    return GetConsoleMode(handle, &mode) != FALSE;
}

} // namespace

void InitializeUtf8Io() {
    if (IsConsoleHandle(STD_OUTPUT_HANDLE) || IsConsoleHandle(STD_ERROR_HANDLE)) {
        if (!SetConsoleOutputCP(CP_UTF8)) {
            throw std::runtime_error("SetConsoleOutputCP(CP_UTF8) failed");
        }
    }
    if (IsConsoleHandle(STD_INPUT_HANDLE)) {
        if (!SetConsoleCP(CP_UTF8)) {
            throw std::runtime_error("SetConsoleCP(CP_UTF8) failed");
        }
    }

    if (!std::setlocale(LC_CTYPE, ".UTF-8")) {
        throw std::runtime_error("Could not initialize the UTF-8 C runtime locale");
    }
    std::locale utf8Locale(std::locale::classic(), ".UTF-8", std::locale::ctype);
    std::wcout.imbue(utf8Locale);
    std::wcerr.imbue(utf8Locale);
    std::wcin.imbue(utf8Locale);
}

std::wstring HResultText(HRESULT hr) {
    wchar_t* buffer = nullptr;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD length = FormatMessageW(
        flags,
        nullptr,
        static_cast<DWORD>(hr),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<wchar_t*>(&buffer),
        0,
        nullptr
    );

    std::wstringstream text;
    text << L"0x" << std::uppercase << std::hex << std::setw(8) << std::setfill(L'0')
         << static_cast<std::uint32_t>(hr);
    if (length && buffer) {
        std::wstring message(buffer, length);
        while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' ')) {
            message.pop_back();
        }
        text << L" (" << message << L")";
    }
    if (buffer) {
        LocalFree(buffer);
    }
    return text.str();
}

std::string Narrow(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    int length = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );
    if (length <= 0) {
        return "<wide-string conversion failed>";
    }
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        length,
        nullptr,
        nullptr
    );
    return result;
}

std::wstring WidenUtf8(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    int length = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0
    );
    if (length <= 0) {
        throw std::runtime_error("Configuration is not valid UTF-8");
    }
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        length
    );
    return result;
}

std::wstring Trim(std::wstring value) {
    auto isSpace = [](wchar_t character) {
        return character == L' ' || character == L'\t' || character == L'\r' || character == L'\n';
    };
    auto first = std::find_if_not(value.begin(), value.end(), isSpace);
    auto last = std::find_if_not(value.rbegin(), value.rend(), isSpace).base();
    if (first >= last) {
        return {};
    }
    return std::wstring(first, last);
}

void Check(HRESULT hr, const wchar_t* operation) {
    if (FAILED(hr)) {
        std::wstringstream message;
        message << operation << L" failed: " << HResultText(hr);
        throw std::runtime_error(Narrow(message.str()));
    }
}

std::wstring GuidText(REFGUID guid) {
    wchar_t buffer[64]{};
    StringFromGUID2(guid, buffer, static_cast<int>(std::size(buffer)));
    return buffer;
}

std::wstring FileTimeText(const FILETIME& fileTime) {
    if (fileTime.dwLowDateTime == 0 && fileTime.dwHighDateTime == 0) {
        return L"(none)";
    }
    SYSTEMTIME utc{};
    if (!FileTimeToSystemTime(&fileTime, &utc)) {
        return L"(invalid FILETIME)";
    }
    wchar_t buffer[64]{};
    swprintf_s(
        buffer,
        L"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
        utc.wYear,
        utc.wMonth,
        utc.wDay,
        utc.wHour,
        utc.wMinute,
        utc.wSecond,
        utc.wMilliseconds
    );
    return buffer;
}

std::wstring VariantTypeText(VARTYPE type) {
    if (type & VT_ARRAY) {
        return L"VT_ARRAY|" + VariantTypeText(type & ~VT_ARRAY);
    }
    switch (type) {
    case VT_EMPTY: return L"VT_EMPTY";
    case VT_NULL: return L"VT_NULL";
    case VT_I1: return L"VT_I1";
    case VT_UI1: return L"VT_UI1";
    case VT_I2: return L"VT_I2";
    case VT_UI2: return L"VT_UI2";
    case VT_I4: return L"VT_I4";
    case VT_UI4: return L"VT_UI4";
    case VT_I8: return L"VT_I8";
    case VT_UI8: return L"VT_UI8";
    case VT_INT: return L"VT_INT";
    case VT_UINT: return L"VT_UINT";
    case VT_R4: return L"VT_R4";
    case VT_R8: return L"VT_R8";
    case VT_BOOL: return L"VT_BOOL";
    case VT_BSTR: return L"VT_BSTR";
    case VT_DATE: return L"VT_DATE";
    case VT_CY: return L"VT_CY";
    case VT_DECIMAL: return L"VT_DECIMAL";
    case VT_ERROR: return L"VT_ERROR";
    default: {
        std::wstringstream stream;
        stream << L"VARTYPE(" << static_cast<unsigned>(type) << L")";
        return stream.str();
    }
    }
}

std::wstring VariantText(const VARIANT& value) {
    if (value.vt & VT_ARRAY) {
        LONG lower = 0;
        LONG upper = -1;
        if (value.parray && SUCCEEDED(SafeArrayGetLBound(value.parray, 1, &lower)) &&
            SUCCEEDED(SafeArrayGetUBound(value.parray, 1, &upper))) {
            std::wstringstream stream;
            stream << L"<array elements=" << (upper - lower + 1) << L">";
            return stream.str();
        }
        return L"<array>";
    }

    switch (value.vt) {
    case VT_EMPTY: return L"<empty>";
    case VT_NULL: return L"<null>";
    case VT_I1: return std::to_wstring(value.cVal);
    case VT_UI1: return std::to_wstring(value.bVal);
    case VT_I2: return std::to_wstring(value.iVal);
    case VT_UI2: return std::to_wstring(value.uiVal);
    case VT_I4:
    case VT_INT: return std::to_wstring(value.lVal);
    case VT_UI4:
    case VT_UINT: return std::to_wstring(value.ulVal);
    case VT_I8: return std::to_wstring(value.llVal);
    case VT_UI8: return std::to_wstring(value.ullVal);
    case VT_R4: {
        std::wstringstream stream;
        stream << value.fltVal;
        return stream.str();
    }
    case VT_R8: {
        std::wstringstream stream;
        stream << value.dblVal;
        return stream.str();
    }
    case VT_BOOL: return value.boolVal == VARIANT_TRUE ? L"true" : L"false";
    case VT_BSTR: return value.bstrVal ? std::wstring(value.bstrVal, SysStringLen(value.bstrVal)) : L"";
    case VT_DATE: {
        SYSTEMTIME systemTime{};
        if (VariantTimeToSystemTime(value.date, &systemTime)) {
            wchar_t buffer[64]{};
            swprintf_s(
                buffer,
                L"%04u-%02u-%02uT%02u:%02u:%02u.%03u",
                systemTime.wYear,
                systemTime.wMonth,
                systemTime.wDay,
                systemTime.wHour,
                systemTime.wMinute,
                systemTime.wSecond,
                systemTime.wMilliseconds
            );
            return buffer;
        }
        return L"<invalid DATE>";
    }
    case VT_ERROR: return HResultText(value.scode);
    default: {
        VARIANT converted{};
        VariantInit(&converted);
        HRESULT hr = VariantChangeType(&converted, const_cast<VARIANT*>(&value), VARIANT_ALPHABOOL, VT_BSTR);
        if (SUCCEEDED(hr) && converted.bstrVal) {
            std::wstring result(converted.bstrVal, SysStringLen(converted.bstrVal));
            VariantClear(&converted);
            return result;
        }
        VariantClear(&converted);
        return L"<unprintable>";
    }
    }
}

std::wstring QualityText(WORD quality) {
    std::wstring major;
    switch (quality & OPC_QUALITY_MASK) {
    case OPC_QUALITY_GOOD: major = L"Good"; break;
    case OPC_QUALITY_UNCERTAIN: major = L"Uncertain"; break;
    default: major = L"Bad"; break;
    }
    std::wstringstream stream;
    stream << major << L" (0x" << std::hex << std::uppercase << std::setw(4)
           << std::setfill(L'0') << quality << L")";
    return stream.str();
}

std::wstring ServerStateText(OPCSERVERSTATE state) {
    switch (state) {
    case OPC_STATUS_RUNNING: return L"RUNNING";
    case OPC_STATUS_FAILED: return L"FAILED";
    case OPC_STATUS_NOCONFIG: return L"NO_CONFIG";
    case OPC_STATUS_SUSPENDED: return L"SUSPENDED";
    case OPC_STATUS_TEST: return L"TEST";
    case OPC_STATUS_COMM_FAULT: return L"COMM_FAULT";
    default: return L"UNKNOWN";
    }
}

} // namespace opcclient
