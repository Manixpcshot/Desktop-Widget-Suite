#pragma once
#include <array>
#include <charconv>
#include <optional>
#include <string_view>
namespace suite {
inline std::optional<std::array<unsigned,3>> version(std::string_view text) {
    if(text.starts_with('v')) text.remove_prefix(1);
    std::array<unsigned,3> result{};
    for(int i=0;i<3;++i) {
        auto end=text.find('.'); auto part=text.substr(0,end);
        if(part.empty() || (part.size()>1 && part.front()=='0')) return {};
        auto parsed=std::from_chars(part.data(),part.data()+part.size(),result[i]);
        if(parsed.ec!=std::errc{} || parsed.ptr!=part.data()+part.size()) return {};
        if(i<2) { if(end==text.npos) return {}; text.remove_prefix(end+1); }
        else if(end!=text.npos) return {};
    }
    return result;
}
inline bool newer(std::string_view candidate,std::string_view current) {
    auto a=version(candidate),b=version(current); return a && b && *a>*b;
}
inline bool validDigest(std::string_view digest) {
    if(!digest.starts_with("sha256:") || digest.size()!=71) return false;
    for(char c:digest.substr(7)) if(!((c>='0' && c<='9') || (c>='a' && c<='f'))) return false;
    return true;
}
}
