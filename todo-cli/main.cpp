#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

struct Todo {
    std::uint32_t id;
    std::string title;
    std::string description;
    bool completed;
};

class TodoStorage {
  public:
    TodoStorage(const std::filesystem::path filepath) : filepath(filepath) {
        if (!std::filesystem::exists(filepath)) {
            std::ofstream{filepath, std::ios::binary};
        }

        std::ifstream file{filepath, std::ios::binary};
        if (!file) {
            throw std::runtime_error("open file");
        }

        std::uint32_t id = 0;
        while (file.read(reinterpret_cast<char *>(&id), sizeof(id))) {
            Todo t;
            t.id = id;

            std::uint32_t str_len{};
            if (!file.read(reinterpret_cast<char *>(&str_len),
                           sizeof(str_len))) {
                std::cerr << "fail read title length" << "\n";
                break;
            }
            t.title.resize(str_len);
            if (!file.read(t.title.data(), str_len)) {
                std::cerr << "fail read title" << "\n";
                break;
            }

            if (!file.read(reinterpret_cast<char *>(&str_len),
                           sizeof(str_len))) {
                std::cerr << "fail read description length" << "\n";
                break;
            }
            t.description.resize(str_len);
            if (!file.read(t.description.data(), str_len)) {
                std::cerr << "fail read description" << "\n";
                break;
            }

            std::uint8_t byte_value{};
            if (!file.read(reinterpret_cast<char *>(&byte_value),
                           sizeof(byte_value))) {
                std::cerr << "fail read completed" << "\n";
                break;
            }
            t.completed = (byte_value != 0);

            todos.push_back(t);
        }

        if (file.eof() || file.fail()) {
            throw std::runtime_error("file is corrupted");
        }
    };

    auto List() { return std::views::all(todos); }

    void Create(Todo todo) {
        todos.push_back(todo);
        save();
    }

    void Update(Todo todo) {
        for (auto &t : todos) {
            if (t.id == todo.id) {
                t = todo;
            }
        }
        save();
    }

    void Delete(const std::uint32_t id) {
        std::erase_if(todos, [id](const Todo &t) { return t.id == id; });
        save();
    }

  private:
    void save() {
        std::ofstream file{filepath, std::ios::binary};

        for (const auto &todo : todos) {
            file.write(reinterpret_cast<const char *>(&todo.id),
                       sizeof(todo.id));

            std::uint32_t title_len =
                static_cast<std::uint32_t>(todo.title.size());
            file.write(reinterpret_cast<const char *>(&title_len),
                       sizeof(title_len));
            file.write(reinterpret_cast<const char *>(todo.title.data()),
                       title_len);

            std::uint32_t description_len =
                static_cast<std::uint32_t>(todo.description.size());
            file.write(reinterpret_cast<const char *>(&description_len),
                       sizeof(description_len));
            file.write(reinterpret_cast<const char *>(todo.description.data()),
                       description_len);

            std::uint8_t byte_value = todo.completed ? 1 : 0;
            file.write(reinterpret_cast<const char *>(&byte_value),
                       sizeof(byte_value));
        }
    }

    const std::filesystem::path filepath;
    std::vector<Todo> todos;
};

std::uint32_t getUint32(const std::string &fieldName) {
    while (true) {
        std::string str_value{};
        std::cout << fieldName << ": ";
        std::getline(std::cin, str_value);
        std::uint32_t value{};
        auto [_, ec] = std::from_chars(
            str_value.data(), str_value.data() + str_value.size(), value);
        if (ec != std::errc()) {
            std::cout << "\n"
                      << "Invalid " << fieldName << " must be an uint32_t"
                      << "\n";
            continue;
        }
        return value;
    }
};

std::string getString(const std::string &fieldName) {
    while (true) {
        std::string value;
        std::cout << fieldName << ": ";
        std::getline(std::cin, value);
        if (value.size() < 3 || value.size() > 512) {
            std::cout << "\n"
                      << "Invalid " << fieldName
                      << " length, must be between 3 and 512 "
                         "inclusive. Try again."
                      << "\n";
            continue;
        }
        return value;
    }
};

bool getBool(const std::string &fieldName) {
    while (true) {
        std::string value;
        std::cout << fieldName << ": ";
        std::getline(std::cin, value);
        if (value != "true" && value != "false") {
            std::cout << "\n"
                      << "Invalid " << fieldName
                      << " must be either 'true' or 'false'."
                         "Try again."
                      << "\n";
            continue;
        }
        return value == "true";
    }
}

Todo getCreateTodo() {
    return {
        .id = 0,
        .title = getString("title"),
        .description = getString("description"),
        .completed = getBool("completed"),
    };
};

Todo getUpdateTodo() {
    return {
        .id = getUint32("id"),
        .title = getString("title"),
        .description = getString("description"),
        .completed = getBool("completed"),
    };
};

int main(int argc, const char *argv[]) {
    if (argc != 2) {
        std::cerr << "not enough arguments to parse command";
        return EXIT_FAILURE;
    }

    std::string_view cmd{argv[1]};

    TodoStorage storage{"todos.bin"};

    if (cmd == "create") {
        auto todo = getCreateTodo();
        for (const auto &t : storage.List()) {
            todo.id = std::max(todo.id, t.id);
        }
        todo.id++;
        storage.Create(todo);
    } else if (cmd == "update") {
        storage.Update(getUpdateTodo());
    } else if (cmd == "delete") {
        auto id = getUint32("id");
        storage.Delete(id);
    } else if (cmd == "list") {
        for (const auto &todo : storage.List()) {
            std::cout << "id: " << todo.id << "\n"
                      << "title: " << todo.title << "\n"
                      << "description: " << todo.description << "\n"
                      << "completed: " << std::boolalpha << todo.completed
                      << "\n";
        }
    } else {
        std::cerr << "invalid command, supported: create, list, update, delete";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
