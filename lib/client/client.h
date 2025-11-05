#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

namespace connection_management {

inline const size_t client_id_size = 10;

class Client {
   public:
    Client();
    std::string Id() const;

   private:
    std::string id_;
};

}  // namespace connection_management