#include <iostream>
#include <string>
#include <vector>
#include <initializer_list>
#include <utility>
#include <cstring>

#include <fcntl.h>
#include <unistd.h>

class FileActions
{
private:
    int* fd_;
    int*& fdRef_;
    unsigned int* ref_count_;
    std::vector<std::pair<std::string, int>> actions_;
public:
    FileActions() = delete;

    FileActions(std::string& path)
        :   fd_(new int(1)),
            fdRef_(fd_),
            ref_count_(new unsigned int(1))
    {
        
        int new_fd = open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
        
        if (new_fd == -1) {
            std::cerr << "[Error] Failed to open file: " << path << std::endl;
            *fd_ = -1;
        } else {
            *fd_ = new_fd; // Store the actual FD number in the heap memory
            std::cout << "[Constructor] Opened " << path << " (FD: " << *fd_ << ")\n";
        }
    }

    FileActions& operator=(const FileActions& other) = delete;


    FileActions(const FileActions& other)
        :   fd_(other.fd_),
            fdRef_(fd_),
            ref_count_(other.ref_count_),
            actions_(other.actions_)
    {
        if (ref_count_) { 
            (*ref_count_)++; 
            std::cout << "[Copy Constructor] Ref count increased to: " << *ref_count_ << "\n";
        }
    }

    void registerActions(std::initializer_list<std::pair<std::string, int>> actions)
    {
        actions_ = actions;
    }

    void executeActions()
    {
        // Check if pointer is valid and file is open
        if (!fd_ || *fdRef_ == -1) {
            std::cerr << "Cannot execute actions: File is not open." << std::endl;
            return;
        }
        
        std::cout << "Executing actions on File Descriptor " << *fdRef_ << ":" << std::endl;
        
        for (const auto& action : actions_) {
            std::string cmd = action.first;
            int val = action.second;

            if (cmd == "write") {
                // Convert int to string + newline
                std::string content = "Value: " + std::to_string(val) + "\n";
                
                ssize_t bytes = write(*fdRef_, content.c_str(), content.size());
                
                if (bytes != -1)
                    std::cout << "  -> [Write] Wrote " << bytes << " bytes (Value: " << val << ")\n";
                else
                    perror("  -> [Write Failed]");
            }
            else if (cmd == "close") {
                // SYSTEM CALL: close
                if (*fdRef_ != -1) {
                    close(*fdRef_);
                    *fdRef_ = -1; // Mark as closed so other copies know
                    std::cout << "  -> [Close] File closed explicitly.\n";
                }
            }
            else {
                std::cout << "  -> [Action] " << cmd << " (Simulated val: " << val << ")\n";
            }
        }
    }
    
    
    ~FileActions() {
        if (ref_count_) {
            (*ref_count_)--; // Decrement the counter
            
            if (*ref_count_ == 0) 
            {
                // Only close if it hasn't been closed yet
                if (fd_ && *fd_ != -1) {
                    std::cout << "[Destructor] Closing file descriptor " << *fd_ << "...\n";
                    close(*fd_);
                }
                
                std::cout << "[Destructor] Ref count is 0. Deleting heap memory.\n";
                delete fd_;        // Delete the int holder
                delete ref_count_; // Delete the counter
                fd_ = nullptr;
                ref_count_ = nullptr;
            } 
            else 
            { 
                std::cout << "[Destructor] Object destroyed. Remaining refs: " << *ref_count_ << "\n"; 
            }
        }
    }
};

int main() {
    std::string path = "data.txt";
    
    FileActions file1(path);
    file1.registerActions({{"write", 100}, {"write", 200}});
    
    {
        std::cout << "\n--- Creating Scope for Copy ---\n";
        FileActions file2 = file1; 
        
        file2.registerActions({{"write", 999}});
        file2.executeActions();
        
    }
    std::cout << "--- Copy Scope Ended ---\n";
    file1.executeActions();
    
    return 0;
}

