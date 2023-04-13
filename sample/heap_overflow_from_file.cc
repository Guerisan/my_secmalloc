#include <iostream>
#include <fstream>
#include <string.h>
#include <malloc.h>

class IFile
{
    public:
    virtual void treatment(char *st) = 0;
};

class File: public IFile
{
    char    buf[20];
    public:
    virtual void treatment(char *st)
    {
        std::cout << "call to Basic::method()" << std::endl;
        std::cout << "we are here " << this << std::endl;
        std::cout << "treat File " << st << std::endl;
        std::ifstream fin;
        fin.open(st);
        if (fin.fail())
        {
            std::cout << "File could'nt open " << std::endl;
        }
        else
        {
            int sz_data;
            fin.read((char*)&sz_data, sizeof(int));
            std::cout << "NB ITEM " << std::dec << (int)sz_data << std::endl;
            fin.read(this->buf, sz_data);
        }
    }

    virtual ~File()
    {}
};

extern "C" {

    void magic_fun()
    {
        std::cout << "magically invokated" << std::endl;
    }

    struct {
        void (*fake_fun)();
    } fake_vtable[] = { magic_fun };
}

int main(int ac, char **av)
{
    static void (*m)() = &magic_fun;
    std::cout << "BEGIN" << std::endl;
    // show adr of magic
    std::cout << "magic_fun " << std::hex << *(long long*) &m << std::endl;
    std::cout << "fake_vptr " << std::hex << fake_vtable << std::endl;
    std::cout << "fake_vptr[0] " << std::hex << *(long long*)&fake_vtable[0] << std::endl;
    // 2 File on heap
    File **inst = (File**) malloc(sizeof(File*) * (ac - 1));
    size_t diff = 0;
    for (int i = 1; i < ac; i += 1)
    {
        inst[i] = new File();
        std::cout << "-----------------------" << std::endl;
        std::cout << "Arg #" << i << std::endl;
        // show where the value of vptr
        std::cout << "vptr " << std::hex << *(long long*)inst[i] << std::endl;
        std::cout << "diff from previous " << diff << std::endl;
        diff = (size_t)inst[i] - diff;
    }
    std::cout << "diff from previous " << diff << std::endl;
    std::cout << "-----------------------" << std::endl;
    for (int i = 1; i < ac; i += 1)
    {
        std::cout << "vptr2 " << std::hex << *(long long*)inst[i] << std::endl;
        // call on parameter
        inst[i]->treatment(av[i]);
    }
    for (int i = 1; i < ac; i += 1)
    {
        std::cout << "Here delete " << std::hex << inst[i] << std::endl;
        delete inst[i];
    }
    free(inst);
}
