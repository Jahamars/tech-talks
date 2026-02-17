from fastapi import FastAPI, HTTPException, Query
from pydantic import BaseModel, Field
from typing import Optional
import psycopg2
import psycopg2.extras
import psycopg2.pool
import os
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = FastAPI(title="HR Management API", version="1.1.0")

# ── Connection pool ───────────────────────────────────────────────────────────
_pool: Optional[psycopg2.pool.SimpleConnectionPool] = None


def get_pool() -> psycopg2.pool.SimpleConnectionPool:
    global _pool
    if _pool is None or _pool.closed:
        _pool = psycopg2.pool.SimpleConnectionPool(
            minconn=1, maxconn=10,
            dbname=os.getenv("DB_NAME", "hrdb"),
            user=os.getenv("DB_USER", "postgres"),
            password=os.getenv("DB_PASSWORD", "postgres"),
            host=os.getenv("DB_HOST", "localhost"),
            port=os.getenv("DB_PORT", "5432"),
        )
    return _pool


def get_conn():
    return get_pool().getconn()


def release_conn(conn):
    get_pool().putconn(conn)


def query(sql: str, params=None, fetch="all"):
    conn = get_conn()
    try:
        cur = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
        cur.execute(sql, params)
        return cur.fetchall() if fetch == "all" else cur.fetchone()
    finally:
        release_conn(conn)


def execute(sql: str, params=None, returning=False):
    conn = get_conn()
    try:
        cur = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
        cur.execute(sql, params)
        result = cur.fetchone() if returning else None
        conn.commit()
        return result
    except Exception:
        conn.rollback()
        raise
    finally:
        release_conn(conn)


# ── Schemas ───────────────────────────────────────────────────────────────────

class DepartmentCreate(BaseModel):
    name: str = Field(..., min_length=1, max_length=100)

class PositionCreate(BaseModel):
    name: str = Field(..., min_length=1, max_length=100)
    salary: float = Field(..., ge=0)

class EmployeeCreate(BaseModel):
    first_name: str = Field(..., min_length=1, max_length=100)
    education_id: int = Field(..., gt=0)
    department_id: int = Field(..., gt=0)
    position_id: int = Field(..., gt=0)

class EducationCreate(BaseModel):
    name: str = Field(..., min_length=1, max_length=100)


# ── Health ────────────────────────────────────────────────────────────────────

@app.get("/health")
def health():
    try:
        conn = get_conn()
        conn.cursor().execute("SELECT 1")
        release_conn(conn)
        return {"status": "ok", "db": "connected"}
    except Exception as e:
        raise HTTPException(status_code=503, detail=f"DB unavailable: {e}")


# ── Departments ───────────────────────────────────────────────────────────────

@app.get("/departments")
def list_departments():
    return query("""
        SELECT d.departmentid, d.departmentname,
               COUNT(e.employeeid) AS employee_count
        FROM departments d
        LEFT JOIN employees e ON d.departmentid = e.departmentid
        GROUP BY d.departmentid, d.departmentname
        ORDER BY d.departmentid
    """)

@app.post("/departments", status_code=201)
def create_department(body: DepartmentCreate):
    return execute(
        "INSERT INTO departments (departmentname) VALUES (%s) RETURNING departmentid, departmentname",
        (body.name,), returning=True)

@app.put("/departments/{dept_id}")
def update_department(dept_id: int, body: DepartmentCreate):
    row = execute(
        "UPDATE departments SET departmentname=%s WHERE departmentid=%s RETURNING departmentid, departmentname",
        (body.name, dept_id), returning=True)
    if not row:
        raise HTTPException(404, "Department not found")
    return row

@app.delete("/departments/{dept_id}", status_code=204)
def delete_department(dept_id: int):
    try:
        execute("DELETE FROM departments WHERE departmentid=%s", (dept_id,))
    except psycopg2.errors.ForeignKeyViolation:
        raise HTTPException(409, "Department has employees")


# ── Positions ─────────────────────────────────────────────────────────────────

@app.get("/positions")
def list_positions():
    return query("SELECT positionid, positionname, salary FROM positions ORDER BY positionid")

@app.post("/positions", status_code=201)
def create_position(body: PositionCreate):
    return execute(
        "INSERT INTO positions (positionname, salary) VALUES (%s,%s) RETURNING positionid, positionname, salary",
        (body.name, body.salary), returning=True)

@app.put("/positions/{pos_id}")
def update_position(pos_id: int, body: PositionCreate):
    row = execute(
        "UPDATE positions SET positionname=%s, salary=%s WHERE positionid=%s RETURNING positionid, positionname, salary",
        (body.name, body.salary, pos_id), returning=True)
    if not row:
        raise HTTPException(404, "Position not found")
    return row

@app.delete("/positions/{pos_id}", status_code=204)
def delete_position(pos_id: int):
    try:
        execute("DELETE FROM positions WHERE positionid=%s", (pos_id,))
    except psycopg2.errors.ForeignKeyViolation:
        raise HTTPException(409, "Position is in use")


# ── Education ─────────────────────────────────────────────────────────────────

@app.get("/education")
def list_education():
    return query("""
        SELECT ed.educationid, ed.educationname,
               COUNT(e.employeeid) AS employee_count
        FROM education ed
        LEFT JOIN employees e ON ed.educationid = e.educationid
        GROUP BY ed.educationid, ed.educationname
        ORDER BY ed.educationid
    """)

@app.post("/education", status_code=201)
def create_education(body: EducationCreate):
    return execute(
        "INSERT INTO education (educationname) VALUES (%s) RETURNING educationid, educationname",
        (body.name,), returning=True)

@app.put("/education/{edu_id}")
def update_education(edu_id: int, body: EducationCreate):
    row = execute(
        "UPDATE education SET educationname=%s WHERE educationid=%s RETURNING educationid, educationname",
        (body.name, edu_id), returning=True)
    if not row:
        raise HTTPException(404, "Education not found")
    return row

@app.delete("/education/{edu_id}", status_code=204)
def delete_education(edu_id: int):
    try:
        execute("DELETE FROM education WHERE educationid=%s", (edu_id,))
    except psycopg2.errors.ForeignKeyViolation:
        raise HTTPException(409, "Education is in use")


# ── Employees ─────────────────────────────────────────────────────────────────

@app.get("/employees")
def list_employees(search: Optional[str] = Query(None, min_length=1)):
    base = """
        SELECT e.employeeid, e.firstname,
               ed.educationname, ed.educationid,
               d.departmentname, d.departmentid,
               p.positionname, p.positionid, p.salary,
               COUNT(h.historyid) AS position_changes
        FROM employees e
        JOIN education ed ON e.educationid = ed.educationid
        JOIN departments d  ON e.departmentid = d.departmentid
        JOIN positions p    ON e.positionid = p.positionid
        LEFT JOIN history h ON e.employeeid = h.employeeid
    """
    if search:
        pat = f"%{search}%"
        return query(base + """
            WHERE e.firstname ILIKE %s OR d.departmentname ILIKE %s OR p.positionname ILIKE %s
            GROUP BY e.employeeid, ed.educationname, ed.educationid,
                     d.departmentname, d.departmentid, p.positionname, p.positionid, p.salary
            ORDER BY e.employeeid
        """, (pat, pat, pat))
    return query(base + """
        GROUP BY e.employeeid, ed.educationname, ed.educationid,
                 d.departmentname, d.departmentid, p.positionname, p.positionid, p.salary
        ORDER BY e.employeeid
    """)

@app.get("/employees/{emp_id}")
def get_employee(emp_id: int):
    row = query("""
        SELECT e.employeeid, e.firstname,
               ed.educationname, ed.educationid,
               d.departmentname, d.departmentid,
               p.positionname, p.positionid, p.salary,
               COUNT(h.historyid) AS position_changes
        FROM employees e
        JOIN education ed ON e.educationid = ed.educationid
        JOIN departments d  ON e.departmentid = d.departmentid
        JOIN positions p    ON e.positionid = p.positionid
        LEFT JOIN history h ON e.employeeid = h.employeeid
        WHERE e.employeeid=%s
        GROUP BY e.employeeid, ed.educationname, ed.educationid,
                 d.departmentname, d.departmentid, p.positionname, p.positionid, p.salary
    """, (emp_id,), fetch="one")
    if not row:
        raise HTTPException(404, "Employee not found")
    return row

@app.post("/employees", status_code=201)
def create_employee(body: EmployeeCreate):
    conn = get_conn()
    cur = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
    try:
        cur.execute(
            "INSERT INTO employees (firstname,educationid,departmentid,positionid) "
            "VALUES (%s,%s,%s,%s) RETURNING employeeid",
            (body.first_name, body.education_id, body.department_id, body.position_id))
        emp_id = cur.fetchone()["employeeid"]
        cur.execute(
            "INSERT INTO history (employeeid,positionid,date) VALUES (%s,%s,CURRENT_DATE)",
            (emp_id, body.position_id))
        conn.commit()
        return {"employeeid": emp_id}
    except psycopg2.errors.ForeignKeyViolation:
        conn.rollback()
        raise HTTPException(400, "Invalid education_id, department_id or position_id")
    except Exception as e:
        conn.rollback()
        raise HTTPException(400, str(e))
    finally:
        release_conn(conn)

@app.put("/employees/{emp_id}")
def update_employee(emp_id: int, body: EmployeeCreate):
    conn = get_conn()
    cur = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
    try:
        cur.execute(
            "UPDATE employees SET firstname=%s,educationid=%s,departmentid=%s,positionid=%s "
            "WHERE employeeid=%s RETURNING employeeid",
            (body.first_name, body.education_id, body.department_id, body.position_id, emp_id))
        if not cur.fetchone():
            raise HTTPException(404, "Employee not found")
        cur.execute("""
            INSERT INTO history (employeeid, positionid, date)
            SELECT %s, %s, CURRENT_DATE
            WHERE NOT EXISTS (
                SELECT 1 FROM history WHERE employeeid=%s AND positionid=%s AND date=CURRENT_DATE
            )
        """, (emp_id, body.position_id, emp_id, body.position_id))
        conn.commit()
        return {"employeeid": emp_id}
    except HTTPException:
        raise
    except psycopg2.errors.ForeignKeyViolation:
        conn.rollback()
        raise HTTPException(400, "Invalid reference id")
    except Exception as e:
        conn.rollback()
        raise HTTPException(400, str(e))
    finally:
        release_conn(conn)

@app.delete("/employees/{emp_id}", status_code=204)
def delete_employee(emp_id: int):
    conn = get_conn()
    cur = conn.cursor()
    try:
        cur.execute("SELECT 1 FROM employees WHERE employeeid=%s", (emp_id,))
        if not cur.fetchone():
            raise HTTPException(404, "Employee not found")
        cur.execute("DELETE FROM history WHERE employeeid=%s", (emp_id,))
        cur.execute("DELETE FROM employees WHERE employeeid=%s", (emp_id,))
        conn.commit()
    except HTTPException:
        raise
    except Exception as e:
        conn.rollback()
        raise HTTPException(400, str(e))
    finally:
        release_conn(conn)

@app.get("/employees/{emp_id}/history")
def employee_history(emp_id: int):
    if not query("SELECT 1 FROM employees WHERE employeeid=%s", (emp_id,), fetch="one"):
        raise HTTPException(404, "Employee not found")
    return query("""
        SELECT h.historyid, h.date, p.positionname, p.salary, e.firstname
        FROM history h
        JOIN employees e ON h.employeeid = e.employeeid
        JOIN positions p ON h.positionid = p.positionid
        WHERE h.employeeid=%s
        ORDER BY h.date DESC
    """, (emp_id,))


# ── History ───────────────────────────────────────────────────────────────────

@app.get("/history")
def list_history():
    return query("""
        SELECT h.historyid, e.firstname AS employee_name, e.employeeid,
               p.positionname, p.salary, h.date
        FROM history h
        JOIN employees e ON h.employeeid = e.employeeid
        JOIN positions p ON h.positionid = p.positionid
        ORDER BY h.date DESC, h.historyid DESC
    """)
