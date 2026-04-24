from flask import Flask, request, render_template, jsonify

app = Flask(__name__)

# in-memory "database"
scores = []


@app.route("/")
def index():
    # sorted by score (highest first)
    sorted_scores = sorted(scores, key=lambda x: x["score"], reverse=True)
    return render_template("index.html", scores=sorted_scores)


@app.route("/submit", methods=["POST"])
def submit():
    data = request.json

    entry = {
        "name": data.get("name", "unknown"),
        "score": int(data.get("score", 0)),
        "attention": float(data.get("attention", 0.0))
    }

    scores.append(entry)
    return {"status": "ok"}


@app.route("/api/scores")
def api_scores():
    return jsonify(scores)


if __name__ == "__main__":
    app.run(debug=True)
