#include "dir.h"

#include <QDir>
#include <QString>
#include <QStringList>
#include <QFileInfo>

Directory::Directory(const char* name) {
    this->dir = new QDir(name);
    QStringList entryList = this->dir->entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    for(int i = 0; i < entryList.size(); i++) {
        QString file_path = entryList.at(i);
        QString file_name = QFileInfo(file_path).fileName();
        this->entries.push_back(file_name.toLatin1().data());
    }
}

bool Directory::good() {
    return this->dir->exists();
}

void Directory::close() {
    this->entries.clear();
    delete this->dir;
}

const char* Directory::read_and_increment() {
    if(this->entry_idx < this->entries.size()) {
        const char* entry = this->entries.at(this->entry_idx);
        this->entry_idx++;
        return entry;
    }
    return nullptr;
}