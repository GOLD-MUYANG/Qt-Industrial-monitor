#pragma once

#include <QString>

class QSqlDatabase;

struct DatabaseMigrationResult
{
    bool success = false;
    int schemaVersion = 0;
    QString errorMessage;
};

class DatabaseMigrator final
{
public:
    static constexpr int CurrentSchemaVersion = 1;

    static DatabaseMigrationResult migrate(QSqlDatabase &database);
};
