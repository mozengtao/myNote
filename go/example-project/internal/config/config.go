// Package config 负责"配置加载"，把 configs/*.json 里的文本变成
// 强类型的 Config 结构体。配置代码属于 package（这里），配置数据
// 属于 configs/ 目录——两者要分开，正如 project_organization.md
// 第五节所说的"配置文件属于 configs，配置代码属于 package"。
package config

import (
	"encoding/json"
	"fmt"
	"os"
)

// PersistenceDriver 决定 internal/app 组装哪种 repository 实现。
type PersistenceDriver string

const (
	DriverMemory PersistenceDriver = "memory"
	DriverMySQL  PersistenceDriver = "mysql"
)

// Config 是整个应用的配置。
//
// 真实项目通常用 YAML（借助 viper 或 gopkg.in/yaml.v3）配置，
// 这里为了保持"零第三方依赖"改用标准库自带的 encoding/json。
type Config struct {
	Port              int               `json:"port"`
	LogLevel          string            `json:"log_level"`
	PersistenceDriver PersistenceDriver `json:"persistence_driver"`
	MySQLDSN          string            `json:"mysql_dsn"`
}

// Load 从指定路径读取 JSON 配置文件。
func Load(path string) (*Config, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("read config file %s: %w", path, err)
	}

	var cfg Config
	if err := json.Unmarshal(data, &cfg); err != nil {
		return nil, fmt.Errorf("parse config file %s: %w", path, err)
	}

	if cfg.Port == 0 {
		cfg.Port = 8080
	}
	if cfg.PersistenceDriver == "" {
		cfg.PersistenceDriver = DriverMemory
	}

	return &cfg, nil
}
