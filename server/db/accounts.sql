USE EndlessPeak;

CREATE TABLE accounts (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    
    login VARCHAR(64) NOT NULL,
    
    password_hash VARBINARY(256) NOT NULL,
    password_salt VARBINARY(64) NOT NULL,
    password_algo VARCHAR(16) NOT NULL,
    
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NULL DEFAULT NULL ON UPDATE CURRENT_TIMESTAMP,

    PRIMARY KEY (id),
    UNIQUE KEY uk_accounts_login (login)
)
ENGINE = InnoDB
DEFAULT CHARSET = utf8mb4
COLLATE = utf8mb4_bin;
