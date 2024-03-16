#include "dir.h"

#include <QDir>
#include <QString>
#include <QStringList>
#include <QFileInfo>

Directory::Directory(const char* name) {
    this->dir = new QDir(name);
}

bool Directory::good() {
    if(this->dir) {
        QDir* dir_instance = reinterpret_cast<QDir*>(this->dir);
        return dir_instance->exists();
    }
    return false;
}

void Directory::close() {
    if(this->dir) {
        QDir* dir_instance = reinterpret_cast<QDir*>(this->dir);
        delete dir_instance;
        this->dir = nullptr;
    }
}

const char* Directory::read_and_increment(){
    if(this->dir) {
        QDir* dir_instance = reinterpret_cast<QDir*>(this->dir);
        QStringList entryList = dir_instance->entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        if(this->entry_idx < entryList.size()) {
            QString file_path = entryList.at(this->entry_idx);
            QString file_name = QFileInfo(file_path).fileName();
            this->entry_idx++;
            return file_name.toStdString().c_str();
        }
    }
    return nullptr;
}