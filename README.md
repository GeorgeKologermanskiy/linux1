# linux1

# Сборка:
build.sh - собирает проект

# Запуск:
./minifs file

# Команды:
ls - вывод корня

ls dir - вывод директории
  
mkdir dir - создать директорию

mkfile file - создать файл
  
rm item - удалить элемент
  
open_file file r/w - открыть файл на чтение / запись. Выдает файловый дескриптор. При открытии на запись затирает файл.
  
close_file fd - закрыть дескриптор
  
write_file fd text - записать в файл по fd соответствующий text в конец
  
read_file fd start len - прочитать по fd начиная со start - len байт
