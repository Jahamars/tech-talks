# hrmanage

HR-система на базе микросервисов: PostgreSQL + FastAPI + nginx/HTML — всё в  Minikube
## Архитектура
```
┌──────────────────────────────────────────────────────────────────┐
│                      Namespace: hrmanage                         │
│                                                                  │
│  ┌──────────────┐   /api/*    ┌────────────────┐                 │
│  │   frontend   │────proxy───▶│  backend-api   │                 │
│  │  nginx:1.27  │             │  FastAPI+Python│                 │
│  │  port 80     │             │  port 8000     │                 │
│  │  NodePort    │             │  ClusterIP     │                 │
│  │  :30080  ◀───┼─── браузер  └───────┬────────┘                 │
│  └──────────────┘                     │                          │
│                                       ▼                          │
│                             ┌──────────────────┐                 │
│                             │    postgres      │                 │
│                             │  postgres:16     │                 │
│                             │  port 5432       │                 │
│                             │  ClusterIP       │                 │
│                             └────────┬─────────┘                 │
│                                      │                           │
│                             ┌────────┴──────────┐                │
│                             │   postgres-pvc    │                │
│                             │   1Gi (данные БД) │                │
│                             └───────────────────┘                │
└──────────────────────────────────────────────────────────────────┘
```

3 микросервиса:
- `postgres` — база данных, ClusterIP (снаружи недоступен)
- `backend-api` — REST API, ClusterIP (доступен только внутри кластера)
- `frontend` — nginx отдаёт UI и проксирует `/api` к backend, NodePort :30080 (снаружи)

## Структура проекта
```
hrmanage/
├── backend/               # Микросервис 1: FastAPI
│   ├── main.py            #   REST API (employees, departments, positions, education, history)
│   ├── requirements.txt   #   Python зависимости
│   └── Dockerfile         #   python:3.12-slim образ
│
├── frontend/              # Микросервис 2: Web UI
│   ├── index.html         #   Single-page приложение (HTML + JS, без фреймворков)
│   ├── nginx.conf         #   nginx конфиг + proxy_pass /api → backend-api
│   └── Dockerfile         #   nginx:1.27-alpine образ
│
├── k8s/                   # Kubernetes манифесты
│   ├── namespace.yaml     #   Namespace hrmanage
│   ├── postgres.yaml      #   ConfigMap (init.sql) + Secret + PVC + Deployment + Service
│   ├── backend.yaml       #   Deployment + Service (ClusterIP)
│   └── frontend.yaml      #   Deployment + Service (NodePort :30080)
│
└── README.md
```

Файлы которых нет и не нужно:
- `info.sql` / `main.sql` — данные живут в `k8s/postgres.yaml` → ConfigMap → `init.sql`
- `docker-compose.yml` — не нужен, всё управляется через kubectl

## Быстрый старт

### 1. Запустить Minikube
```bash
minikube start
```

### 2. Переключить Docker в окружение Minikube
```bash
eval $(minikube docker-env)
```
> Это важно — образы собираются сразу внутри Minikube, `minikube image load` не нужен.

### 3. Собрать образы
```bash
docker build -t backend-api:latest ./backend
docker build -t frontend:latest ./frontend
```

### 4. Задеплоить всё
```bash
kubectl apply -f k8s/namespace.yaml
kubectl apply -f k8s/postgres.yaml
kubectl apply -f k8s/backend.yaml
kubectl apply -f k8s/frontend.yaml
```

### 5. Дождаться готовности
```bash
kubectl get pods -n hrmanage -w
```
Ждём пока все поды станут `Running`

### 6. Открыть в браузере
```bash
# Web UI
echo "http://$(minikube ip):30080"

# API Swagger
echo "http://$(minikube ip):30080/api/docs"
```

## Что доступно

| URL | Описание |
|-----|----------|
| `http://<ip>:30080` | Web UI — управление сотрудниками |
| `http://<ip>:30080/api/docs` | Swagger — документация и тестирование API |
| `http://<ip>:30080/api/health` | Health check API + БД |

> `<ip>` — результат команды `minikube ip` 

## API эндпоинты

| Метод | URL | Описание |
|-------|-----|----------|
| GET | `/api/health` | Health check |
| GET/POST | `/api/employees` | Список / создание сотрудников |
| GET/PUT/DELETE | `/api/employees/{id}` | Один сотрудник |
| GET | `/api/employees/{id}/history` | История должностей |
| GET/POST | `/api/departments` | Отделы |
| PUT/DELETE | `/api/departments/{id}` | Изменить/удалить отдел |
| GET/POST | `/api/positions` | Должности |
| PUT/DELETE | `/api/positions/{id}` | Изменить/удалить должность |
| GET/POST | `/api/education` | Уровни образования |
| PUT/DELETE | `/api/education/{id}` | Изменить/удалить |
| GET | `/api/history` | Полная история изменений |

## Пересборка после изменений
```bash
eval $(minikube docker-env)

# Пересобрать backend
docker build -t backend-api:latest ./backend
kubectl rollout restart deployment/backend-api -n hrmanage

# Пересобрать frontend
docker build -t frontend:latest ./frontend
kubectl rollout restart deployment/frontend -n hrmanage

# Статус
kubectl rollout status deployment/backend-api -n hrmanage
kubectl rollout status deployment/frontend -n hrmanage
```

## Полный сброс и перезапуск
```bash
kubectl delete namespace hrmanage
kubectl apply -f k8s/namespace.yaml -f k8s/postgres.yaml -f k8s/backend.yaml -f k8s/frontend.yaml
```

## Отладка
```bash
# Статус всех ресурсов
kubectl get all -n hrmanage

# Логи
kubectl logs -n hrmanage -l app=backend-api -f
kubectl logs -n hrmanage -l app=frontend -f
kubectl logs -n hrmanage -l app=postgres -f

# Зайти в postgres
kubectl exec -it -n hrmanage \
  $(kubectl get pod -n hrmanage -l app=postgres -o jsonpath='{.items[0].metadata.name}') \
  -- psql -U postgres -d hrdb

# Описание пода 
kubectl describe pod -n hrmanage -l app=backend-api
```
