#include "common/utils_random.hpp"

namespace naivertc {
namespace utils {
namespace random {

std::string random_string(int max_length) {
    static std::string possible_characters("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
    std::random_device rd;
    std::mt19937 engine(rd());
    std::uniform_int_distribution<int> dist(0, possible_characters.size()-1);
    std::string ret = "";
    for(int i = 0; i < max_length; i++){
        int random_index = dist(engine); //get index between 0 and possible_characters.size()-1
        ret += possible_characters[random_index];
    }
    return ret;
}

} // namespace random
} // namespace utils
} // naivertc