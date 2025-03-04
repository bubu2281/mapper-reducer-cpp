#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <pthread.h>
#include <iostream>
#include <fstream>
#include <string>
#include <set>
#include <vector>
#include <algorithm>
#include <sstream>
#include <map>


typedef struct {
    int id;
    std::map<std::string, std::set<int>> *hashmap;
    std::vector<std::string> *filePaths;
    pthread_mutex_t *mutex_mapper_files;
    pthread_mutex_t *mutex_mapper_hashmap;
    pthread_barrier_t *barrier_mapper_reducer;

} Mapper;

typedef struct {
    int id;
    std::map<std::string, std::set<int>> *hashmap;
    std::vector<char> *alphabet;
    pthread_barrier_t *barrier_mapper_reducer;
    pthread_mutex_t *mutex_alphabet;
} Reducer;

void *f_mapper(void *arg) {
    Mapper mapper = *(Mapper *) arg;
    while (1) {
        pthread_mutex_lock(mapper.mutex_mapper_files);
        if ((*mapper.filePaths).empty()) {
            pthread_mutex_unlock(mapper.mutex_mapper_files);
            break;
        }
        std::string filePath = *prev((*mapper.filePaths).end());
        size_t fileIndex = mapper.filePaths->size();
        (*mapper.filePaths).pop_back();
        pthread_mutex_unlock(mapper.mutex_mapper_files);
        std::ifstream file(filePath);
        if (!file) {
            std::cout << "nu s-a putu deschide fisierul  " << filePath << "\n";
            continue;
        }

        std::map<std::string, std::set<int>> localMap;

        std::string line;
        while (std::getline(file, line)) {
            std::istringstream stream(line);
            std::string token;

            while (stream >> token) { 
                std::transform(token.begin(), token.end(), token.begin(), ::tolower);

                token.erase(std::remove_if(token.begin(), token.end(), [](char c) {
                    return !std::isalpha(static_cast<unsigned char>(c));
                }), token.end());

                if (!token.empty()) {
                    localMap[token].insert(fileIndex);
                }
            }
        }
        
        pthread_mutex_lock(mapper.mutex_mapper_hashmap);
        for (const auto &entry : localMap)
            (*mapper.hashmap)[entry.first].insert(entry.second.begin(), entry.second.end());
        pthread_mutex_unlock(mapper.mutex_mapper_hashmap);

        file.close(); 
    }
    pthread_barrier_wait(mapper.barrier_mapper_reducer);
    return NULL;
}


struct CompareBySetSize {
    bool operator()(const std::set<int>& a, const std::set<int>& b) const {
        return a.size() > b.size();
    }
};

void *f_reducer(void *arg) {
    Reducer reducer = *(Reducer *) arg;
    pthread_barrier_wait(reducer.barrier_mapper_reducer);
    while (1) {
        pthread_mutex_lock(reducer.mutex_alphabet);
        if ((*reducer.alphabet).empty()) {
            pthread_mutex_unlock(reducer.mutex_alphabet);
            break;
        }

        char letter = *prev((*reducer.alphabet).end());
        (*reducer.alphabet).pop_back();
        pthread_mutex_unlock(reducer.mutex_alphabet);


        std::multimap<std::set<int>, std::string, CompareBySetSize> letterMap;

        for (auto &el : *(reducer.hashmap)) {
            if (el.first[0] == letter) {
                letterMap.insert({el.second, el.first});
            }
        }

        std::string filename = std::string(1, letter) + ".txt";

        std::ofstream outfile(filename, std::ios::app);

        for (auto &el : letterMap) {
            outfile << el.second << ":[";
            for (auto &el2 : el.first) {
                outfile << el2;
                if (el.first.find(el2) != el.first.end() && next(el.first.find(el2)) != el.first.end()) {
                    outfile << " ";
                }
            }
            outfile << "]\n";
        } 

        outfile.close();


    }
    return NULL;
}


int main(int argc, char **argv)
{
    if (argc != 4) {
        printf("Format invalid: ./tema1 <NUM_MAPPER> <NUM_REDUCER> <INPUT_FILE>\n");
        fflush(stdout);
        return 0;
    }
    int num_mappers, num_reducers;
    char input_file[100];

    num_mappers = atoi(argv[1]);
    num_reducers = atoi(argv[2]);
    strcpy(input_file, argv[3]);

    std::vector<char> alphabet;

    for (char c = 'a'; c <= 'z'; ++c) {
        std::string filename = std::string(1, c) + ".txt";
        std::ofstream file(filename);
        if (file.is_open()) {
            file.close(); 
        }
        alphabet.push_back(c);
    }
    std::ifstream inputFile(input_file);
    if (!inputFile) {
        std::cout << "nu s-a putut deschide fisierul de input";
        return 1;
    }

    int numFiles;
    inputFile >> numFiles;

    std::vector<std::string> filePaths(numFiles);
    for (int i = 0; i < numFiles; i++) {
        inputFile >> filePaths[i];
        filePaths[i] = "../checker/" + filePaths[i];
    }

    inputFile.close(); 

    for (const std::string& filePath : filePaths) {
        std::ifstream file(filePath);
        if (!file) {
            std::cout << "nu s-a putu deschide fisierul";
            continue;
        }

        std::string line;
        while (std::getline(file, line)) {
        }

        file.close(); 
    }

    int num_threads = num_mappers + num_reducers;

    Mapper mappers[num_mappers];
    Reducer reducers[num_reducers];

   
    std::map<std::string, std::set<int>> hashmap;
    pthread_mutex_t mutex_mapper_files = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t mutex_mapper_hashmap = PTHREAD_MUTEX_INITIALIZER;

    pthread_mutex_t mutex_alphabet = PTHREAD_MUTEX_INITIALIZER;

    pthread_barrier_t barrier_mapper_reducer;
    pthread_barrier_init(&barrier_mapper_reducer, nullptr, num_threads); 

    pthread_t threads[num_threads];
    int r;
    for (int id = 0; id < num_threads; id++) {

        if (id < num_mappers) {
            mappers[id].filePaths = &filePaths;
            mappers[id].hashmap = &hashmap;
            mappers[id].mutex_mapper_files = &mutex_mapper_files;
            mappers[id].mutex_mapper_hashmap = &mutex_mapper_hashmap;
            mappers[id].barrier_mapper_reducer = &barrier_mapper_reducer;
            mappers[id].id = id;
            r = pthread_create(&threads[id], NULL, f_mapper, &mappers[id]);
        } else {
            reducers[id - num_mappers].id = id - num_mappers;
            reducers[id - num_mappers].hashmap = &hashmap;
            reducers[id - num_mappers].alphabet = &alphabet;
            reducers[id - num_mappers].barrier_mapper_reducer = &barrier_mapper_reducer;
            reducers[id - num_mappers].mutex_alphabet = &mutex_alphabet;
            r = pthread_create(&threads[id], NULL, f_reducer, &reducers[id - num_mappers]);
        }

        if (r) {
            printf("Eroare la crearea thread-ului %d\n", id);
            exit(-1);
        }
    }

    void *status;
    for (int id = 0; id < num_threads; id++) {
        r = pthread_join(threads[id], &status);

        if (r) {
        printf("Eroare la asteptarea thread-ului %d\n", id);
        exit(-1);
        }
    }

    pthread_barrier_destroy(&barrier_mapper_reducer);
    pthread_mutex_destroy(&mutex_mapper_files);
    pthread_mutex_destroy(&mutex_mapper_hashmap);
    pthread_mutex_destroy(&mutex_alphabet);
    return 0;
}