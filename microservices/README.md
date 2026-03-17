microservice hr system: postgresql + fastapi + nginx/html — all in minikube

```
┌──────────────────────────────────────────────────────────────────┐
│                      namespace: hrmanage                         │
│                                                                  │
│  ┌──────────────┐   /api/*    ┌────────────────┐                 │
│  │   frontend   │────proxy───▶│  backend-api   │                 │
│  │  nginx:1.27  │             │  fastapi+python│                 │
│  │  port 80     │             │  port 8000     │                 │
│  │  nodeport    │             │  clusterip     │                 │
│  │  :30080  ◀───┼─── browser  └───────┬────────┘                 │
│  └──────────────┘                     │                          │
│                                       ▼                          │
│                             ┌──────────────────┐                 │
│                             │    postgres      │                 │
│                             │  postgres:16     │                 │
│                             │  port 5432       │                 │
│                             │  clusterip       │                 │
│                             └────────┬─────────┘                 │
│                                      │                           │
│                             ┌────────┴──────────┐                │
│                             │   postgres-pvc    │                │
│                             │   1gi (db data)   │                │
│                             └───────────────────┘                │
└──────────────────────────────────────────────────────────────────┘
```

3 microservices:
- `postgres` — database, clusterip (not reachable from outside)
- `backend-api` — rest api, clusterip (only inside cluster)
- `frontend` — nginx serves ui and proxies `/api` to backend, nodeport :30080 (reachable)

```
hrmanage/
├── backend/               # fastapi service
│   ├── main.py            #   rest api (employees, departments, positions, education, history)
│   ├── requirements.txt   #   python deps
│   └── dockerfile         #   python:3.12-slim base
│
├── frontend/              # web ui
│   ├── index.html         #   single page (html + js, no frameworks)
│   ├── nginx.conf         #   nginx config + proxy_pass /api → backend-api
│   └── dockerfile         #   nginx:1.27-alpine base
│
├── k8s/                   # kubernetes manifests
│   ├── namespace.yaml     #   namespace hrmanage
│   ├── postgres.yaml      #   configmap (init.sql) + secret + pvc + deployment + service
│   ├── backend.yaml       #   deployment + service (clusterip)
│   └── frontend.yaml      #   deployment + service (nodeport :30080)
│
└── readme.md
```

quick start
1) start minikube
```bash
minikube start
```
2) point docker to minikube env
```bash
eval $(minikube docker-env)
```
> important: images build inside minikube, no `minikube image load` needed.
3) build images
```bash
docker build -t backend-api:latest ./backend
docker build -t frontend:latest ./frontend
```

4) deploy everything
```bash
kubectl apply -f k8s/namespace.yaml
kubectl apply -f k8s/postgres.yaml
kubectl apply -f k8s/backend.yaml
kubectl apply -f k8s/frontend.yaml
```

5) wait until ready
```bash
kubectl get pods -n hrmanage -w
```
wait for all pods to be `running`

6) open in browser
```bash
# web ui
echo "http://$(minikube ip):30080"

# api swagger
echo "http://$(minikube ip):30080/api/docs"
```

what’s available

| url | description |
|-----|-------------|
| `http://<ip>:30080` | web ui — manage employees |
| `http://<ip>:30080/api/docs` | swagger — api docs and testing |
| `http://<ip>:30080/api/health` | health check api + db |

> `<ip>` — result of `minikube ip`

api endpoints
| method | url | description |
|-------|-----|-------------|
| get | `/api/health` | health check |
| get/post | `/api/employees` | list / create employees |
| get/put/delete | `/api/employees/{id}` | single employee |
| get | `/api/employees/{id}/history` | position history |
| get/post | `/api/departments` | departments |
| put/delete | `/api/departments/{id}` | update/delete department |
| get/post | `/api/positions` | positions |
| put/delete | `/api/positions/{id}` | update/delete position |
| get/post | `/api/education` | education levels |
| put/delete | `/api/education/{id}` | update/delete |
| get | `/api/history` | full change history |

rebuild after changes
```bash
eval $(minikube docker-env)

# rebuild backend
docker build -t backend-api:latest ./backend
kubectl rollout restart deployment/backend-api -n hrmanage

# rebuild frontend
docker build -t frontend:latest ./frontend
kubectl rollout restart deployment/frontend -n hrmanage

# status
kubectl rollout status deployment/backend-api -n hrmanage
kubectl rollout status deployment/frontend -n hrmanage
```

full reset and restart
```bash
kubectl delete namespace hrmanage
kubectl apply -f k8s/namespace.yaml -f k8s/postgres.yaml -f k8s/backend.yaml -f k8s/frontend.yaml
```

debug
```bash
# all resources
kubectl get all -n hrmanage

# logs
kubectl logs -n hrmanage -l app=backend-api -f
kubectl logs -n hrmanage -l app=frontend -f
kubectl logs -n hrmanage -l app=postgres -f

# enter postgres
kubectl exec -it -n hrmanage \
  $(kubectl get pod -n hrmanage -l app=postgres -o jsonpath='{.items[0].metadata.name}') \
  -- psql -U postgres -d hrdb

# describe pod 
kubectl describe pod -n hrmanage -l app=backend-api
```
