# cpp-search-server
## Финальный проект: поисковый сервер

Поисковый сервер состоит из нескольких пользовательских классов:

- Concurrent map — класс, который позволяет использовать параллельную обработку словаря, разбивая его на подсловари;
- Document — структура описывающая документ, которая содердит поля: индетификационный номер, рейтинг и релевантность;
- Log Duration — класс, замеряющий время выполнения участков кода, который использует для сравнения эффективности кода;
- Paginator — класс с помощью которого происходит разбивка документов на страницы с документами;
- Request Queue — объединяет методы обработки запросов;
- Search Server — класс, реализующий поисковой сервер, содержит следующий функицонал:
    - Методы для добавления документов;
    - Методы для удаления документов;
    - Методы для выполнения поисковых запросов с различными параметрами, такими как статус документа, либо особый предикат;
    - Реализация поиска с использованием различных политик выполнения (последовательное, параллельное);
- TestRunner — класс, используемый для юнит-тестирования проекта.

### Системные требования

Для корректной работы проекта необходимо выполнить следующие действия:

Компилируйте, активировав как минимум 17-й стандарт: подойдут флаги --std=c++17, --std=c++2a или --std=c++20.

### Windows

Возьмите самый свежий архив из [Release versions](https://winlibs.com), без пометки «without LLVM/Clang/LLD/LLDB». После распаковки используйте g++ из папки mingw64\bin.

### Unix

Установите следующие пакеты:
- g++-9 — свежий компилятор;
- libstdc++-9-dev — свежая реализация стандартной библиотеки;
- libtbb-dev — вспомогательная библиотека Thread Building Blocks от Intel для реализации параллельности. На macOS используйте [brew](https://github.com/mxcl/homebrew-made?ysclid=loy74xae8q331219416) install tbb.
Подойдут и более новые версии.

Подключите нужные библиотеки, поставив флаги -ltbb и -lpthread.
