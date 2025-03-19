// Implementation of the KMeans Algorithm
// reference: https://github.com/marcoscastro/kmeans

#define TBB_PREVIEW_GLOBAL_CONTROL 1 
#include <iostream>
#include <vector>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/mutex.h>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/global_control.h>

using namespace std;

class Point
{
private:
	int id_point, id_cluster;
	vector<double> values;
	int total_values;
	string name;

public:
	Point(int id_point, vector<double>& values, string name = "")
	{
		this->id_point = id_point;
		total_values = values.size();

		for(int i = 0; i < total_values; i++)
			this->values.push_back(values[i]);

		this->name = name;
		id_cluster = -1;
	}

	int getID()
	{
		return id_point;
	}

	void setCluster(int id_cluster)
	{
		this->id_cluster = id_cluster;
	}

	int getCluster()
	{
		return id_cluster;
	}

	double getValue(int index)
	{
		return values[index];
	}

	int getTotalValues()
	{
		return total_values;
	}

	void addValue(double value)
	{
		values.push_back(value);
	}

	string getName()
	{
		return name;
	}
};

class Cluster
{
private:
	int id_cluster;
	vector<double> central_values;
	vector<Point> points;
    //Locking points within each cluster 
    tbb::mutex points_lock;

public:
	Cluster(int id_cluster, Point point)
	{
		this->id_cluster = id_cluster;

		int total_values = point.getTotalValues();

		for(int i = 0; i < total_values; i++)
			central_values.push_back(point.getValue(i));

		points.push_back(point);
	}

	void addPoint(Point point)
	{
        tbb::mutex::scoped_lock lock(points_lock);
		points.push_back(point);
	}

	bool removePoint(int id_point)
	{
		int total_points = points.size();
        tbb::mutex::scoped_lock lock(points_lock);
		for(int i = 0; i < total_points; i++)
		{
			if(points[i].getID() == id_point)
			{
				points.erase(points.begin() + i);
				return true;
			}
		}
		return false;
	}

	double getCentralValue(int index)
	{
		return central_values[index];
	}

	void setCentralValue(int index, double value)
	{
		central_values[index] = value;
	}

	Point getPoint(int index)
	{
		return points[index];
	}

	int getTotalPoints()
	{
		return points.size();
	}

	int getID()
	{
		return id_cluster;
	}
};

class KMeans
{
private:
    int K; // number of clusters
    int total_values, total_points, max_iterations;
    vector<unique_ptr<Cluster>> clusters; 

    // return ID of nearest center (uses euclidean distance)
    int getIDNearestCenter(Point point)
    {
        //Thread local pair to store the minimum distance and the cluster id
        tbb::enumerable_thread_specific<std::pair<double, int>> local_min(
            []() { return std::make_pair(numeric_limits<double>::max(), 0); });

        //Calculate the distance between the point and each cluster center in parallel
        tbb::parallel_for(0, K, [&](int i) {
            double sum = 0.0;

            for (int j = 0; j < total_values; j++)
            {
                double diff = clusters[i]->getCentralValue(j) - point.getValue(j);
                sum += diff * diff;
            }

            //Originally used a lock to protect this section and performance was equal or worse than serial
            auto& local_data = local_min.local();
            if (sum < local_data.first)
            {
                local_data.first = sum;
                local_data.second = i;
            }
        });

        //Combine results from all threads to find the minimum
        double min_dist = numeric_limits<double>::max();
        int id_cluster_center = 0;

        for (const auto& local_data : local_min)
        {
            if (local_data.first < min_dist)
            {
                min_dist = local_data.first;
                id_cluster_center = local_data.second;
            }
        }

        return id_cluster_center;
    }

public:
    KMeans(int K, int total_points, int total_values, int max_iterations)
    {
        this->K = K;
        this->total_points = total_points;
        this->total_values = total_values;
        this->max_iterations = max_iterations;
    }

    void run(vector<Point>& points) 
    {
        auto begin = chrono::high_resolution_clock::now();

        if (K > total_points)
            return;

        vector<int> prohibited_indexes;

        // choose K distinct values for the centers of the clusters
        for (int i = 0; i < K; i++)
        {
            while (true)
            {
                int index_point = rand() % total_points;

                if (find(prohibited_indexes.begin(), prohibited_indexes.end(), index_point) == prohibited_indexes.end())
                {
                    prohibited_indexes.push_back(index_point);
                    points[index_point].setCluster(i);
                    //Changed to use the unique_ptr
                    clusters.push_back(make_unique<Cluster>(i, points[index_point])); 
                    break;
                }
            }
        }
        auto end_phase1 = chrono::high_resolution_clock::now();

        int iter = 1;

        while (true)
        {
            bool done = true;

            // associates each point to the nearest center in parallel
            tbb::parallel_for(0, total_points, [&](int i) {
                int id_old_cluster = points[i].getCluster();
                int id_nearest_center = getIDNearestCenter(points[i]);

                if (id_old_cluster != id_nearest_center)
                {
                    //Remove the point from the old cluster and add it to the new one
                    if (id_old_cluster != -1)
                        clusters[id_old_cluster]->removePoint(points[i].getID());

                    points[i].setCluster(id_nearest_center);
                    clusters[id_nearest_center]->addPoint(points[i]);
                    done = false;
                }
            });

            // recalculating the center of each cluster in parallel
            tbb::parallel_for(0, K, [&](int i) {
                for (int j = 0; j < total_values; j++)
                {
                    int total_points_cluster = clusters[i]->getTotalPoints();
                    double sum = 0.0;

                    if (total_points_cluster > 0)
                    {
                        for (int p = 0; p < total_points_cluster; p++)
                            sum += clusters[i]->getPoint(p).getValue(j);

                        clusters[i]->setCentralValue(j, sum / total_points_cluster);
                    }
                }
            });

            if (done == true || iter >= max_iterations)
            {
                cout << "Break in iteration " << iter << "\n\n";
                break;
            }

            iter++;
        }
        auto end = chrono::high_resolution_clock::now();

        // shows elements of clusters
        for (int i = 0; i < K; i++)
        {
            int total_points_cluster = clusters[i]->getTotalPoints();

            cout << "Cluster " << clusters[i]->getID() + 1 << endl;
            for (int j = 0; j < total_points_cluster; j++)
            {
                cout << "Point " << clusters[i]->getPoint(j).getID() + 1 << ": ";
                for (int p = 0; p < total_values; p++)
                    cout << clusters[i]->getPoint(j).getValue(p) << " ";

                string point_name = clusters[i]->getPoint(j).getName();

                if (point_name != "")
                    cout << "- " << point_name;

                cout << endl;
            }

            cout << "Cluster values: ";

            for (int j = 0; j < total_values; j++)
                cout << clusters[i]->getCentralValue(j) << " ";

            cout << "\n\n";
            cout << "TOTAL EXECUTION TIME = " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << "\n";

            cout << "TIME PHASE 1 = " << std::chrono::duration_cast<std::chrono::microseconds>(end_phase1 - begin).count() << "\n";

            cout << "TIME PHASE 2 = " << std::chrono::duration_cast<std::chrono::microseconds>(end - end_phase1).count() << "\n";
        }
        ofstream outfile("ouput.txt", std::ios::app);
        if (outfile.is_open())
        {
            outfile << "Break in iteration " << iter << "\n";
            outfile << "TOTAL EXECUTION TIME = " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << "\n";
            outfile << "TIME PHASE 1 = " << std::chrono::duration_cast<std::chrono::microseconds>(end_phase1 - begin).count() << "\n";
            outfile << "TIME PHASE 2 = " << std::chrono::duration_cast<std::chrono::microseconds>(end - end_phase1).count() << "\n";
            outfile.close();
        }
        else
        {
            cerr << "Unable to open file";
        }
    }
};

int main(int argc, char *argv[])
{
    //tbb::global_control control(tbb::global_control::max_allowed_parallelism, 64);
	srand (79);

	int total_points, total_values, K, max_iterations, has_name;

	cin >> total_points >> total_values >> K >> max_iterations >> has_name;

	vector<Point> points;
	string point_name;

	for(int i = 0; i < total_points; i++)
	{
		vector<double> values;

		for(int j = 0; j < total_values; j++)
		{
			double value;
			cin >> value;
			values.push_back(value);
		}

		if(has_name)
		{
			cin >> point_name;
			Point p(i, values, point_name);
			points.push_back(p);
		}
		else
		{
			Point p(i, values);
			points.push_back(p);
		}
	}

	KMeans kmeans(K, total_points, total_values, max_iterations);
	kmeans.run(points);

	return 0;
}